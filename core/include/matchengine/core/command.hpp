#pragma once

#include <matchengine/core/order.hpp>

#include <variant>

namespace matchengine {

using NewOrder = Order;

struct CancelOrder {
    OrderId order_id{};

    bool operator==(const CancelOrder&) const = default;
};

using Command = std::variant<NewOrder, CancelOrder>;

} // namespace matchengine
