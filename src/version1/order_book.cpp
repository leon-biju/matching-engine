#include "matching/version1/order_book.hpp"

#include <iterator>

namespace matching::version1 {

bool OrderBook::side_empty(OrderSide side) const {
    return with_side(side, [](const auto& levels) { return levels.empty(); });
}

bool OrderBook::can_fill(OrderSide aggressor_side, std::optional<Price> limit, Quantity qty) const {
    return with_opposite(aggressor_side, [&](const auto& levels) {
        // Count down rather than summing resting quantity, which could overflow.
        for (const auto& [price, queue] : levels) {
            if (!crosses(aggressor_side, limit, price)) {
                return false;
            }
            for (const Order& resting : queue) {
                qty -= std::min(qty, resting.remaining_qty);
                if (qty == 0) {
                    return true;
                }
            }
        }
        return false;
    });
}

OrderBook::Handle OrderBook::add(const Order& order) {
    Level& queue = with_side(order.side, [&](auto& levels) -> Level& { return levels[order.price]; });
    queue.push_back(order);
    return std::prev(queue.end());
}

void OrderBook::remove(Handle handle) {
    with_side(handle->side, [&](auto& levels) {
        auto level = levels.find(handle->price);
        level->second.erase(handle);
        if (level->second.empty()) {
            levels.erase(level);
        }
    });
}

} // namespace matching::version1
