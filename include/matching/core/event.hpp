#pragma once

#include <variant>
#include <vector>

#include "matching/core/types.hpp"

namespace matching {

// New orders are checked in this order and rejected with the first reason that applies.
enum class RejectReason {
    DuplicateOrderId,
    UnknownSymbol,
    InvalidQuantity,
    InvalidPrice,
    NoLiquidity,  // Market order when the opposite side's book is empty
    UnknownOrder, // Cancel for an order that is not resting
};

struct OrderAccepted {
    OrderId id;

    bool operator==(const OrderAccepted&) const = default;
};

struct OrderRejected {
    OrderId id;
    RejectReason reason;

    bool operator==(const OrderRejected&) const = default;
};

struct Trade {
    OrderId aggressor_id;
    OrderId resting_id;
    OrderSide aggressor_side;
    Price price;
    Quantity quantity;

    bool operator==(const Trade&) const = default;
};

struct OrderRested {
    OrderId id;
    Price price;
    Quantity remaining;

    bool operator==(const OrderRested&) const = default;
};

struct OrderCancelled {
    OrderId id;
    Quantity remaining; // Quantity removed from the book or left unfilled

    bool operator==(const OrderCancelled&) const = default;
};

struct CancelRejected {
    OrderId id;
    RejectReason reason;

    bool operator==(const CancelRejected&) const = default;
};

using Event =
    std::variant<OrderAccepted, OrderRejected, Trade, OrderRested, OrderCancelled, CancelRejected>;

// All events caused by a single command, stamped with that command's engine sequence.
struct EventBatch {
    Sequence sequence;
    std::vector<Event> events;

    bool operator==(const EventBatch&) const = default;
};

} // namespace matching
