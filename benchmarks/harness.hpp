#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "matching/core/event.hpp"
#include "matching/core/event_sink.hpp"
#include "matching/core/types.hpp"
#include "matching/workload/workload_file.hpp"

// Helpers shared by the benchmark tools.

namespace matching::benchmarks {

// Counts events without storing them, so the sink adds as little as possible to the timings.
class CountingSink {
  public:
    void begin_batch(Sequence) {}
    void add(const Event&) { ++events_; }
    void end_batch() {}

    std::uint64_t events() const { return events_; }

  private:
    std::uint64_t events_ = 0;
};

static_assert(EventSink<CountingSink>);

// The arguments every benchmark tool takes: <workload file> [repeats].
struct Arguments {
    std::string workload_path;
    int repeats = 5;
};

// Parses the arguments, or prints the usage or error and returns nothing.
inline std::optional<Arguments> parse_arguments(int argc, char** argv, std::string_view tool) {
    if (argc != 2 && argc != 3) {
        std::cerr << "usage: " << tool << " <workload file> [repeats]\n";
        return std::nullopt;
    }
    Arguments arguments{.workload_path = argv[1]};
    if (argc == 3) {
        const std::string_view text = argv[2];
        const auto [end, error] =
            std::from_chars(text.data(), text.data() + text.size(), arguments.repeats);
        if (error != std::errc{} || end != text.data() + text.size() || arguments.repeats < 1) {
            std::cerr << "repeats must be a positive integer\n";
            return std::nullopt;
        }
    }
    return arguments;
}

// Loads a workload file with at least one command and prints its size, or prints the error and
// returns nothing.
inline std::optional<workload::Workload> load_workload(const std::string& path) {
    workload::Workload workload;
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::cerr << "cannot open " << path << '\n';
            return std::nullopt;
        }
        workload = workload::read_workload(in);
    } catch (const std::exception& error) {
        std::cerr << path << ": " << error.what() << '\n';
        return std::nullopt;
    }
    if (workload.commands.empty()) {
        std::cerr << path << " has no commands\n";
        return std::nullopt;
    }
    std::cout << path << ": " << workload.commands.size() << " commands, " << workload.symbol_count
              << " symbols\n";
    return workload;
}

// The best and median of the rates from repeated runs, in commands per second.
struct Throughput {
    double best = 0;
    double median = 0;
    std::uint64_t events = 0;
};

inline Throughput summarise(std::vector<double> rates, std::uint64_t events) {
    std::ranges::sort(rates);
    return {.best = rates.back(), .median = rates[rates.size() / 2], .events = events};
}

inline void print_throughput(const Throughput& throughput, int repeats) {
    std::cout << std::fixed << std::setprecision(2) << "  throughput: best "
              << throughput.best / 1e6 << "M, median " << throughput.median / 1e6
              << "M commands/s over " << repeats << " runs (" << throughput.events << " events)\n";
}

// The nearest-rank percentile of sorted, non-empty values.
inline std::int64_t percentile(const std::vector<std::int64_t>& sorted, double p) {
    const auto rank = static_cast<std::size_t>(std::ceil(p / 100 * sorted.size()));
    return sorted[std::max<std::size_t>(rank, 1) - 1];
}

// Prints the p50, p90, p99, p99.9 and maximum of sorted, non-empty latencies in nanoseconds.
inline void print_latency(const std::vector<std::int64_t>& sorted) {
    std::cout << "  latency ns:";
    for (const double p : {50.0, 90.0, 99.0, 99.9}) {
        std::cout << " p" << std::setprecision(p == 99.9 ? 1 : 0) << p << ' '
                  << percentile(sorted, p);
    }
    std::cout << " max " << sorted.back() << '\n';
}

} // namespace matching::benchmarks
