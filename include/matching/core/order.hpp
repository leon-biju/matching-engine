#pragma once

#include "matching/core/types.hpp"

namespace matching {

// An order resting on the book. Only LimitGTC orders rest, so price is always set.
struct Order {
    OrderId id;
    SymbolId symbol;
    OrderSide side;
    Price price;
    Quantity remaining_qty;
    Sequence sequence; // Arrival order, used for time priority
};

} // namespace matching
