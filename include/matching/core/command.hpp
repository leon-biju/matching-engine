#pragma once

#include <optional>
#include <variant>

#include "matching/core/types.hpp"

namespace matching {

struct NewOrder {
    OrderId id;
    SymbolId symbol; // Mapped from the symbol name by the command source
    OrderSide side;
    OrderType type;
    Quantity qty;
    std::optional<Price> price; // Required for limit orders, absent for market orders
};

struct CancelOrder {
    OrderId id;
};

using EngineCommand = std::variant<NewOrder, CancelOrder>;

} // namespace matching
