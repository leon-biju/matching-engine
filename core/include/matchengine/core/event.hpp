#pragma once

#include <matchengine/core/types.hpp>

#include <optional>
#include <variant>

namespace matchengine {

enum class OrderRejectReason {
    UnknownSymbol = 1,
    DuplicateOrderId = 2,
    InvalidQuantity = 3,
    InvalidPrice = 4,
    InvalidSide = 5,
    InvalidOrderType = 6,
    InsufficientLiquidity = 7,
};

enum class CancelRejectReason { UnknownOrder = 1, OrderNotActive = 2 };
enum class CancelReason { UserRequested = 1, IOCExpired = 2 };

struct OrderAccepted {
    OrderId order_id{};
    OrderSide side{OrderSide::Buy};
    OrderType type{OrderType::LimitGTC};
    Quantity original_qty{};
    std::optional<Price> price;
    bool operator==(const OrderAccepted&) const = default;
};

struct OrderRejected {
    OrderId order_id{};
    OrderRejectReason reason{};
    bool operator==(const OrderRejected&) const = default;
};

struct Trade {
    OrderId maker_order_id{};
    OrderId taker_order_id{};
    Price price{};
    Quantity qty{};
    OrderSide aggressor_side{OrderSide::Buy};
    bool operator==(const Trade&) const = default;
};

struct OrderRested {
    OrderId order_id{};
    Price price{};
    Quantity remaining_qty{};
    bool operator==(const OrderRested&) const = default;
};

struct OrderCancelled {
    OrderId order_id{};
    Quantity remaining_qty{};
    CancelReason reason{};
    bool operator==(const OrderCancelled&) const = default;
};

struct CancelRejected {
    OrderId order_id{};
    CancelRejectReason reason{};
    bool operator==(const CancelRejected&) const = default;
};

using EventPayload = std::variant<OrderAccepted, OrderRejected, Trade, OrderRested, OrderCancelled, CancelRejected>;

struct Event {
    EventIndex event_index{};
    std::optional<Symbol> symbol;
    EventPayload payload;
    bool operator==(const Event&) const = default;
};

} // namespace matchengine
