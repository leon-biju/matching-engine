#pragma once

#include <cstdint>

namespace matching {

using OrderId = std::uint64_t;
using Price = std::uint64_t; // Atomic currency units (e.g. pence or cents)
using Quantity = std::uint64_t;
using Sequence = std::uint64_t; // Engine sequence, starts at 1
using SymbolId = std::uint32_t; // Index into the engine's fixed symbol set

enum class OrderSide { Buy, Sell };

enum class OrderType { LimitGTC, LimitIOC, LimitFOK, MarketIOC, MarketFOK };

} // namespace matching
