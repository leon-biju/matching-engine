#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <system_error>
#include <vector>

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

namespace {

using Clock = std::chrono::steady_clock;

struct Throughput {
    double best = 0;   // Commands per second
    double median = 0; // Commands per second
    std::uint64_t events = 0;
};

// Runs the whole workload through a fresh engine `repeats` times. The event count is printed, so
// the compiler cannot drop the work, and it should match between engines.
template <Engine E> Throughput measure_throughput(const workload::Workload& workload, int repeats) {
    std::vector<double> rates;
    std::uint64_t events = 0;
    for (int run = 0; run < repeats; ++run) {
        E engine(workload.symbol_count);
        events = 0;
        const auto start = Clock::now();
        for (const EngineCommand& command : workload.commands) {
            events += engine.process(command).events.size();
        }
        const std::chrono::duration<double> elapsed = Clock::now() - start;
        rates.push_back(static_cast<double>(workload.commands.size()) / elapsed.count());
    }
    std::ranges::sort(rates);
    return {.best = rates.back(), .median = rates[rates.size() / 2], .events = events};
}

// Times each command on its own through one fresh engine, in nanoseconds, sorted. Like the
// throughput pass, each time includes destroying the batch.
template <Engine E> std::vector<std::int64_t> measure_latency(const workload::Workload& workload) {
    std::vector<std::int64_t> latencies;
    latencies.reserve(workload.commands.size());
    E engine(workload.symbol_count);
    for (const EngineCommand& command : workload.commands) {
        const auto start = Clock::now();
        engine.process(command);
        const auto end = Clock::now();
        latencies.push_back(std::chrono::nanoseconds(end - start).count());
    }
    std::ranges::sort(latencies);
    return latencies;
}

// The nearest-rank percentile of sorted, non-empty values.
std::int64_t percentile(const std::vector<std::int64_t>& sorted, double p) {
    const auto rank = static_cast<std::size_t>(std::ceil(p / 100 * sorted.size()));
    return sorted[std::max<std::size_t>(rank, 1) - 1];
}

template <Engine E>
void benchmark(std::string_view name, const workload::Workload& workload, int repeats) {
    const Throughput throughput = measure_throughput<E>(workload, repeats);
    std::cout << name << '\n'
              << std::fixed << std::setprecision(2) << "  throughput: best "
              << throughput.best / 1e6 << "M, median " << throughput.median / 1e6
              << "M commands/s over " << repeats << " runs (" << throughput.events << " events)\n";

    const std::vector<std::int64_t> latencies = measure_latency<E>(workload);
    std::cout << "  latency ns:";
    for (const double p : {50.0, 90.0, 99.0, 99.9}) {
        std::cout << " p" << std::setprecision(p == 99.9 ? 1 : 0) << p << ' '
                  << percentile(latencies, p);
    }
    std::cout << " max " << latencies.back() << '\n';
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: engine_benchmark <workload file> [repeats]\n";
        return 2;
    }
    int repeats = 5;
    if (argc == 3) {
        const std::string_view text = argv[2];
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), repeats);
        if (error != std::errc{} || end != text.data() + text.size() || repeats < 1) {
            std::cerr << "repeats must be a positive integer\n";
            return 2;
        }
    }

    workload::Workload workload;
    try {
        std::ifstream in(argv[1], std::ios::binary);
        if (!in) {
            std::cerr << "cannot open " << argv[1] << '\n';
            return 1;
        }
        workload = workload::read_workload(in);
    } catch (const std::exception& error) {
        std::cerr << argv[1] << ": " << error.what() << '\n';
        return 1;
    }
    if (workload.commands.empty()) {
        std::cerr << argv[1] << " has no commands\n";
        return 1;
    }
    std::cout << argv[1] << ": " << workload.commands.size() << " commands, "
              << workload.symbol_count << " symbols\n";

    // Add each new engine here.
    benchmark<version1::ReferenceEngine>("version1", workload, repeats);
    return 0;
}
