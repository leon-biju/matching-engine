#include <charconv>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <string_view>
#include <system_error>

#include "matching/workload/command_generator.hpp"
#include "matching/workload/workload_file.hpp"

// Writes a workload file of generated commands, so every benchmark run replays the same stream.
//
// Usage: make_workload <profile> <seed> <command count> <output file>

using namespace matching;

namespace {

std::optional<std::uint64_t> parse_number(std::string_view text) {
    std::uint64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: make_workload <profile> <seed> <command count> <output file>\n"
                     "profiles: realistic, aggressive, test\n";
        return 2;
    }
    const auto config = workload::profiles::by_name(argv[1]);
    const auto seed = parse_number(argv[2]);
    const auto count = parse_number(argv[3]);
    if (!config) {
        std::cerr << "unknown profile: " << argv[1] << '\n';
        return 2;
    }
    if (!seed || !count) {
        std::cerr << "seed and command count must be non-negative integers\n";
        return 2;
    }

    workload::CommandGenerator generator(*seed, *config);
    workload::Workload result{.symbol_count = config->symbol_count, .commands = {}};
    result.commands.reserve(*count);
    for (std::uint64_t i = 0; i < *count; ++i) {
        result.commands.push_back(generator.next());
    }

    std::ofstream out(argv[4], std::ios::binary);
    if (!out) {
        std::cerr << "cannot open " << argv[4] << '\n';
        return 1;
    }
    try {
        workload::write_workload(out, result);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "wrote " << *count << " commands to " << argv[4] << '\n';
    return 0;
}
