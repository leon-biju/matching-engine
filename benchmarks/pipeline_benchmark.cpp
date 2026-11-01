#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

#include "harness.hpp"
#include "matching/core/command.hpp"
#include "matching/core/engine.hpp"
#include "matching/core/event.hpp"
#include "matching/core/event_sink.hpp"
#include "matching/core/types.hpp"
#include "matching/pipeline/queue_sink.hpp"
#include "matching/pipeline/spsc_queue.hpp"
#include "matching/version1/engine.hpp"
#include "matching/workload/workload_file.hpp"

// Replays a workload file through every engine as a full pipeline: a source thread queues the
// commands, the engine thread processes them, and a sink thread reads the events, with an SPSC
// queue between each pair. It reports throughput, and the latency from queueing each command to
// its batch reaching the sink. The file is loaded and each engine is built before any timing
// starts.
//
// Usage: pipeline_benchmark <workload file> [repeats]

using namespace matching;
using namespace matching::benchmarks;
using namespace matching::pipeline;

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t command_queue_capacity = 4096;
constexpr std::size_t event_queue_capacity = 16384;

// The latency pass paces the source at this fraction of the median throughput. See
// measure_latency for why.
constexpr double latency_load = 0.5;

struct Queues {
    SpscQueue<EngineCommand> commands{command_queue_capacity};
    SpscQueue<SinkItem> events{event_queue_capacity};
};

// Receives the batches on the sink thread. It counts events, checks that batches arrive in
// sequence order, and, when given the enqueue times, records each command's latency.
class PipelineSink {
  public:
    PipelineSink() = default;

    // enqueued[i] is when the source queued the command with sequence i + 1. The sink writes that
    // command's latency to latencies[i].
    PipelineSink(const std::vector<Clock::time_point>& enqueued,
                 std::vector<std::int64_t>& latencies)
        : enqueued_(&enqueued), latencies_(&latencies) {}

    void begin_batch(Sequence sequence) {
        if (sequence != current_ + 1) {
            in_order_ = false;
        }
        current_ = sequence;
    }
    void add(const Event&) { ++events_; }
    void end_batch() {
        // Once a batch is out of order, the sequence no longer gives the command's index.
        if (latencies_ != nullptr && in_order_) {
            // The source wrote enqueued[i] before pushing the command. That push released it, the
            // engine thread's pop acquired it, the engine's pushes of this batch released it again,
            // and this thread's pops acquired them. So the write happens before this read.
            const std::size_t index = current_ - 1;
            (*latencies_)[index] =
                std::chrono::nanoseconds(Clock::now() - (*enqueued_)[index]).count();
        }
    }

    std::uint64_t events() const { return events_; }
    bool in_order() const { return in_order_; }

  private:
    const std::vector<Clock::time_point>* enqueued_ = nullptr;
    std::vector<std::int64_t>* latencies_ = nullptr;
    Sequence current_ = 0; // A fresh engine numbers its commands from 1
    std::uint64_t events_ = 0;
    bool in_order_ = true;
};

static_assert(EventSink<PipelineSink>);

// Runs the whole workload through a fresh engine E on three threads. The source calls
// before_push(i) just before it queues command i. Returns the seconds from releasing the threads
// to the sink receiving the last batch, which excludes starting the threads.
template <Engine E, typename BeforePush>
double run_pipeline(const workload::Workload& workload, BeforePush before_push,
                    PipelineSink& sink) {
    E engine(workload.symbol_count);
    const auto queues = std::make_unique<Queues>();
    const std::size_t count = workload.commands.size();

    std::atomic<bool> go{false};
    const auto wait_for_go = [&go] {
        Backoff backoff;
        while (!go.load(std::memory_order_acquire)) {
            backoff.wait();
        }
    };

    std::thread source([&] {
        wait_for_go();
        for (std::size_t i = 0; i < count; ++i) {
            before_push(i);
            queues->commands.push(workload.commands[i]);
        }
    });
    std::thread matcher([&] {
        wait_for_go();
        QueueSink out(queues->events);
        for (std::size_t i = 0; i < count; ++i) {
            const EngineCommand command = queues->commands.pop();
            engine.process(command, out);
        }
    });
    Clock::time_point end;
    std::thread consumer([&] {
        wait_for_go();
        std::size_t batches = 0;
        while (batches < count) {
            if (replay(queues->events.pop(), sink)) {
                ++batches;
            }
        }
        end = Clock::now();
    });

    const auto start = Clock::now();
    go.store(true, std::memory_order_release);
    source.join();
    matcher.join();
    consumer.join();
    return std::chrono::duration<double>(end - start).count();
}

// Runs the pipeline `repeats` times with the source queueing commands as fast as it can.
template <Engine E>
std::optional<Throughput> measure_throughput(const workload::Workload& workload, int repeats) {
    std::vector<double> rates;
    std::uint64_t events = 0;
    for (int run = 0; run < repeats; ++run) {
        PipelineSink sink;
        const double seconds = run_pipeline<E>(workload, [](std::size_t) {}, sink);
        if (!sink.in_order()) {
            return std::nullopt;
        }
        events = sink.events();
        rates.push_back(static_cast<double>(workload.commands.size()) / seconds);
    }
    return summarise(rates, events);
}

// Runs the pipeline once and returns each command's latency in nanoseconds, sorted: from the
// source queueing it to the sink receiving its end_batch.
//
// The source is paced to queue commands evenly at `rate` commands per second. A source that runs
// flat out keeps the command queue full, so each latency would mostly be the time spent behind
// a full queue of earlier commands. Pacing below the pipeline's throughput keeps the queues
// mostly empty, so the latency shows the handoffs and the matching, plus the queueing from bursts
// of slow commands.
template <Engine E>
std::optional<std::vector<std::int64_t>> measure_latency(const workload::Workload& workload,
                                                         double rate) {
    const std::size_t count = workload.commands.size();
    std::vector<Clock::time_point> enqueued(count);
    std::vector<std::int64_t> latencies(count);
    const double interval_ns = 1e9 / rate;
    Clock::time_point origin;

    const auto before_push = [&](std::size_t i) {
        if (i == 0) {
            origin = Clock::now();
        }
        const auto due = origin + std::chrono::nanoseconds(std::llround(interval_ns * i));
        Clock::time_point now = Clock::now();
        while (now < due) {
            now = Clock::now();
        }
        enqueued[i] = now;
    };

    PipelineSink sink(enqueued, latencies);
    run_pipeline<E>(workload, before_push, sink);
    if (!sink.in_order()) {
        return std::nullopt;
    }
    std::ranges::sort(latencies);
    return latencies;
}

template <Engine E>
bool benchmark(std::string_view name, const workload::Workload& workload, int repeats) {
    std::cout << name << '\n';
    const std::optional<Throughput> throughput = measure_throughput<E>(workload, repeats);
    if (!throughput) {
        std::cerr << name << ": batches reached the sink out of sequence order\n";
        return false;
    }
    print_throughput(*throughput, repeats);

    const double rate = throughput->median * latency_load;
    std::cout << "  latency pass: source paced at " << rate / 1e6 << "M commands/s\n";
    const auto latencies = measure_latency<E>(workload, rate);
    if (!latencies) {
        std::cerr << name << ": batches reached the sink out of sequence order\n";
        return false;
    }
    print_latency(*latencies);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    const std::optional<Arguments> arguments = parse_arguments(argc, argv, "pipeline_benchmark");
    if (!arguments) {
        return 2;
    }
    const std::optional<workload::Workload> workload = load_workload(arguments->workload_path);
    if (!workload) {
        return 1;
    }

    // Add each new engine here.
    if (!benchmark<version1::ReferenceEngine>("version1", *workload, arguments->repeats)) {
        return 1;
    }
    return 0;
}
