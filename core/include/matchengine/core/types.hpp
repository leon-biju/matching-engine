#pragma once

#include <cstdint>
#include <string>

namespace matchengine {

using OrderId = std::uint64_t;

// potential todo: switch this to a fixed array although SSO should help us for most symbols
using Symbol = std::string;

using Price = std::uint64_t;
using Quantity = std::uint64_t;
using EventIndex = std::uint64_t;

enum class OrderSide { Buy, Sell };
enum class OrderType { LimitGTC, LimitIOC, LimitFOK, MarketIOC, MarketFOK };

} // namespace matchengine
