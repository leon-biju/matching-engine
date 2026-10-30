#pragma once

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <vector>

#include "matching/core/event.hpp"
#include "matching/core/order.hpp"
#include "matching/core/types.hpp"

namespace matching::version1 {

// The resting orders for one particular symbol.
// one symbol. Each price level is a FIFO queue, so time priority is the
// position in the queue.
class OrderBook {
public:
    // This is used as a FIFO Queue. Using a std::list so we can easily remove middle elements
    // Also reallocation won't make handles point to deallocated memory
    using Level = std::list<Order>;
    using Handle = Level::iterator;

    bool side_empty(OrderSide side) const;

    // Whether enough quantity crosses `limit` to fill `qty` completely`
    bool can_fill(OrderSide aggressor_side, std::optional<Price> limit, Quantity qty) const;


    // Matches an incoming order against best opposite order.
    // One trade event per resting order touched
    // Call on_filled when a resting order is fully filled just before it leaves the book.
    // Returns the incoming order's unfilled quantity.
    template <typename OnFilled>
    Quantity match(OrderId aggressor_id, OrderSide aggressor_side, std::optional<Price> limit,
                   Quantity qty, std::vector<Event>& events, OnFilled&& on_filled);

    Handle add(const Order& order);
    void remove(Handle handle);

private:
    static bool crosses(OrderSide aggressor_side, std::optional<Price> limit, Price resting_price) {
        if (!limit) {
            return true;
        }
        return aggressor_side == OrderSide::Buy ? resting_price <= *limit : resting_price >= *limit;
    }

    // bids_ and asks_ have different types, so these pick one and pass it to a generic `f`.
    template <typename Self, typename F>
    decltype(auto) with_side(this Self&& self, OrderSide side, F&& f) {
        return side == OrderSide::Buy ? f(self.bids_) : f(self.asks_);
    }

    // The levels an aggressor on `side` matches against.
    template <typename Self, typename F>
    decltype(auto) with_opposite(this Self&& self, OrderSide side, F&& f) {
        return side == OrderSide::Buy ? f(self.asks_) : f(self.bids_);
    }

    std::map<Price, Level, std::greater<>> bids_; // Best (highest) bid first
    std::map<Price, Level> asks_;                 // Best (lowest) ask first
};

template <typename OnFilled>
Quantity OrderBook::match(OrderId aggressor_id, OrderSide aggressor_side,
                          std::optional<Price> limit, Quantity qty, std::vector<Event>& events,
                          OnFilled&& on_filled) {
    return with_opposite(aggressor_side, [&](auto& levels) {
        while (qty > 0 && !levels.empty()) {
            auto level = levels.begin();
            auto& [price, queue] = *level;
            if (!crosses(aggressor_side, limit, price)) {
                break;
            }

            while (qty > 0 && !queue.empty()) {
                Order& resting = queue.front();
                const Quantity fill = std::min(qty, resting.remaining_qty);
                events.push_back(Trade{
                    .aggressor_id = aggressor_id,
                    .resting_id = resting.id,
                    .aggressor_side = aggressor_side,
                    .price = price,
                    .quantity = fill,
                });
                qty -= fill;
                resting.remaining_qty -= fill;
                if (resting.remaining_qty == 0) {
                    on_filled(resting);
                    queue.pop_front();
                }
            }

            if (queue.empty()) {
                levels.erase(level);
            }
        }
        return qty;
    });
}

} // namespace matching::version1
