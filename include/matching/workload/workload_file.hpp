#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "matching/core/command.hpp"
#include "matching/core/types.hpp"

namespace matching::workload {

// A recorded command stream and the symbol count to build the engine with.
struct Workload {
    std::size_t symbol_count = 0;
    std::vector<EngineCommand> commands;

    bool operator==(const Workload&) const = default;
};

// The workload file format. Every integer is little-endian, so a file reads the same on any
// platform. Commands that the engine rejects, such as a limit order without a price, are kept
// as they are, so a file can record any stream.
//
// Header, 24 bytes:
//   0  magic "MEWL"
//   4  u32 format version (1)
//   8  u64 symbol count
//   16 u64 command count
//
// Each command, 32 bytes:
//   0  u8  kind: 0 NewOrder, 1 CancelOrder
//   1  u8  side, as OrderSide
//   2  u8  type, as OrderType
//   3  u8  1 if the order has a price, else 0
//   4  u32 symbol ID
//   8  u64 order ID
//   16 u64 quantity
//   24 u64 price, 0 when absent
//
// A CancelOrder sets only the kind and order ID, and leaves the other bytes 0.
namespace format {

inline constexpr std::array<char, 4> magic{'M', 'E', 'W', 'L'};
inline constexpr std::uint32_t version = 1;
inline constexpr std::size_t header_size = 24;
inline constexpr std::size_t command_size = 32;

} // namespace format

namespace detail {

template <typename T> void put(unsigned char* out, T value) {
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        out[i] = static_cast<unsigned char>(value >> (8 * i));
    }
}

template <typename T> T get(const unsigned char* in) {
    T value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value |= static_cast<T>(in[i]) << (8 * i);
    }
    return value;
}

inline void read_exactly(std::istream& in, unsigned char* out, std::size_t size) {
    in.read(reinterpret_cast<char*>(out), static_cast<std::streamsize>(size));
    if (static_cast<std::size_t>(in.gcount()) != size) {
        throw std::runtime_error("workload file is truncated");
    }
}

} // namespace detail

// Writes `workload` in the format above. Throws std::runtime_error if the stream fails.
inline void write_workload(std::ostream& out, const Workload& workload) {
    std::array<unsigned char, format::header_size> header{};
    for (std::size_t i = 0; i < format::magic.size(); ++i) {
        header[i] = static_cast<unsigned char>(format::magic[i]);
    }
    detail::put<std::uint32_t>(&header[4], format::version);
    detail::put<std::uint64_t>(&header[8], workload.symbol_count);
    detail::put<std::uint64_t>(&header[16], workload.commands.size());
    out.write(reinterpret_cast<const char*>(header.data()), header.size());

    for (const EngineCommand& command : workload.commands) {
        std::array<unsigned char, format::command_size> record{};
        if (const auto* order = std::get_if<NewOrder>(&command)) {
            record[0] = 0;
            record[1] = static_cast<unsigned char>(order->side);
            record[2] = static_cast<unsigned char>(order->type);
            record[3] = order->price.has_value() ? 1 : 0;
            detail::put<std::uint32_t>(&record[4], order->symbol);
            detail::put<std::uint64_t>(&record[8], order->id);
            detail::put<std::uint64_t>(&record[16], order->qty);
            detail::put<std::uint64_t>(&record[24], order->price.value_or(0));
        } else {
            record[0] = 1;
            detail::put<std::uint64_t>(&record[8], std::get<CancelOrder>(command).id);
        }
        out.write(reinterpret_cast<const char*>(record.data()), record.size());
    }

    if (!out) {
        throw std::runtime_error("failed to write workload file");
    }
}

// Reads a workload written by write_workload. Throws std::runtime_error if the file is
// truncated, has the wrong magic or version, or holds a field value no command can have.
inline Workload read_workload(std::istream& in) {
    std::array<unsigned char, format::header_size> header{};
    detail::read_exactly(in, header.data(), header.size());
    for (std::size_t i = 0; i < format::magic.size(); ++i) {
        if (header[i] != static_cast<unsigned char>(format::magic[i])) {
            throw std::runtime_error("not a workload file");
        }
    }
    if (const auto version = detail::get<std::uint32_t>(&header[4]); version != format::version) {
        throw std::runtime_error("unsupported workload file version " + std::to_string(version));
    }

    Workload workload{.symbol_count = detail::get<std::uint64_t>(&header[8]), .commands = {}};
    const auto count = detail::get<std::uint64_t>(&header[16]);

    for (std::uint64_t i = 0; i < count; ++i) {
        std::array<unsigned char, format::command_size> record{};
        detail::read_exactly(in, record.data(), record.size());
        const auto id = detail::get<std::uint64_t>(&record[8]);

        if (record[0] == 1) {
            workload.commands.push_back(CancelOrder{.id = id});
            continue;
        }
        if (record[0] != 0 || record[1] > 1 || record[2] > 4 || record[3] > 1) {
            throw std::runtime_error("invalid command record " + std::to_string(i));
        }
        workload.commands.push_back(NewOrder{
            .id = id,
            .symbol = detail::get<std::uint32_t>(&record[4]),
            .side = static_cast<OrderSide>(record[1]),
            .type = static_cast<OrderType>(record[2]),
            .qty = detail::get<std::uint64_t>(&record[16]),
            .price = record[3] == 1 ? std::optional<Price>{detail::get<std::uint64_t>(&record[24])}
                                    : std::nullopt,
        });
    }
    return workload;
}

} // namespace matching::workload
