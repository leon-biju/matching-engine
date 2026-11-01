#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

#include "harness.hpp"
#include "matching/core/command.hpp"
#include "matching/core/engine.hpp"
#include "matching/version1/engine.hpp"
#include "matching/workload/workload_file.hpp"

// Replays a workload file through every engine and reports throughput and per-command latency.
// The file is loaded and each engine is built before any timing starts, so only process() is
// measured. The two are measured in separate passes, because reading the clock around every
// command slows the throughput pass down.
//
// Usage: engine_benchmark <workload file> [repeats]

using namespace matching;
using namespace matching::benchmarks;

namespace {

using Clock = std::chrono::steady_clock;

// Runs the whole workload through a fresh engine `repeats` times. The event count is printed, so
// the compiler cannot drop the work, and it should match between engines.
template <Engine E> Throughput measure_throughput(const workload::Workload& workload, int repeats) {
    std::vector<double> rates;
    std::uint64_t events = 0;
    for (int run = 0; run < repeats; ++run) {
        E engine(workload.symbol_count);
        CountingSink sink;
        const auto start = Clock::now();
        for (const EngineCommand& command : workload.commands) {
            engine.process(command, sink);
        }
        const std::chrono::duration<double> elapsed = Clock::now() - start;
        events = sink.events();
        rates.push_back(static_cast<double>(workload.commands.size()) / elapsed.count());
    }
    return summarise(rates, events);
}

// Times each command on its own through one fresh engine, in nanoseconds, sorted.
template <Engine E> std::vector<std::int64_t> measure_latency(const workload::Workload& workload) {
    std::vector<std::int64_t> latencies;
    latencies.reserve(workload.commands.size());
    E engine(workload.symbol_count);
    CountingSink sink;
    for (const EngineCommand& command : workload.commands) {
        const auto start = Clock::now();
        engine.process(command, sink);
        const auto end = Clock::now();
        latencies.push_back(std::chrono::nanoseconds(end - start).count());
    }
    std::ranges::sort(latencies);
    return latencies;
}

template <Engine E>
void benchmark(std::string_view name, const workload::Workload& workload, int repeats) {
    std::cout << name << '\n';
    print_throughput(measure_throughput<E>(workload, repeats), repeats);
    print_latency(measure_latency<E>(workload));
}

} // namespace

int main(int argc, char** argv) {
    const std::optional<Arguments> arguments = parse_arguments(argc, argv, "engine_benchmark");
    if (!arguments) {
        return 2;
    }
    const std::optional<workload::Workload> workload = load_workload(arguments->workload_path);
    if (!workload) {
        return 1;
    }

    // Add each new engine here.
    benchmark<version1::ReferenceEngine>("version1", *workload, arguments->repeats);
    return 0;
}
