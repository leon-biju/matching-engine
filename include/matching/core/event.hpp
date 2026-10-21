#pragma once

#include <variant>
#include <vector>

#include "matching/core/types.hpp"

namespace matching {

enum class RejectReason {
    UnknownSymbol,
    DuplicateOrderId,
    InvalidPrice,
    InvalidQuantity,
    UnknownOrder, // Cancel for an order that is not resting
};

struct OrderAccepted {
    OrderId id;
};

struct OrderRejected {
    OrderId id;
    RejectReason reason;
};

struct Trade {
    OrderId aggressor_id;
    OrderId resting_id;
    OrderSide aggressor_side;
    Price price;
    Quantity quantity;
};

struct OrderRested {
    OrderId id;
    Price price;
    Quantity remaining;
};

struct OrderCancelled {
    OrderId id;
    Quantity remaining; // Quantity removed from the book or left unfilled
};

struct CancelRejected {
    OrderId id;
    RejectReason reason;
};

using Event =
    std::variant<OrderAccepted, OrderRejected, Trade, OrderRested, OrderCancelled, CancelRejected>;

// All events caused by a single command, stamped with that command's engine sequence.
struct EventBatch {
    Sequence sequence;
    std::vector<Event> events;
};

} // namespace matching
