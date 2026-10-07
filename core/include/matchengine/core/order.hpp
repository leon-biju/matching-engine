#pragma once

#include <matchengine/core/types.hpp>

#include <optional>

namespace matchengine {

struct Order {
    OrderId order_id{};
    Symbol symbol;
    OrderSide side{OrderSide::Buy};
    OrderType type{OrderType::LimitGTC};
    Quantity qty{};
    std::optional<Price> price;

    bool operator==(const Order&) const = default;

};

} // namespace matchengine
