#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

#include "matching/core/command.hpp"
#include "matching/core/types.hpp"

namespace matching::workload {

// Shapes a random command stream. Chances are in thousandths, so 10 means 1%.
struct GeneratorConfig {
    std::size_t symbol_count = 4; // Valid symbol IDs are 0 to symbol_count - 1
    Price mid_price = 1'000;
    Price price_range = 10;     // Limit prices are mid_price +/- price_range; keep below mid_price
    Quantity max_quantity = 50; // Quantities are 1 to max_quantity

    // Relative weights of the order types, indexed by OrderType.
    std::array<unsigned, 5> type_weights{8, 2, 2, 1, 1};

    unsigned cancel_chance = 250;        // A command is a CancelOrder
    unsigned cancel_random_chance = 200; // A cancel names a random ID, not a GTC order it sent
    // Cancels wait until at least this many sent GTC orders are left to name, so books can grow.
    std::size_t cancel_pool_min = 0;
    // A GTC order is priced on its own side of mid_price (buys below, sells above), so it rests.
    unsigned passive_chance = 0;
    unsigned unknown_symbol_chance = 10; // A new order has a symbol ID outside the valid range
    unsigned zero_quantity_chance = 10;
    unsigned bad_price_chance = 10; // A limit order without a valid price, or a market with one
    unsigned reused_id_chance = 10; // A new order reuses an ID it sent before

    bool operator==(const GeneratorConfig&) const = default;
};

// Named configs for benchmark workloads. The defaults above suit the invariant tests, which want
// every rejection often; these send only well-formed commands, so the engine spends its time
// matching. Cancels can still name filled orders, and market orders can still find an empty book.
namespace profiles {

// Mostly passive GTC orders over a wide price range, with heavy cancelling once books are deep.
// Books take roughly the first 50,000 commands to fill up.
inline constexpr GeneratorConfig realistic{
    .symbol_count = 8,
    .mid_price = 10'000,
    .price_range = 100,
    .max_quantity = 100,
    .type_weights{12, 3, 1, 2, 1},
    .cancel_chance = 400,
    .cancel_random_chance = 0,
    .cancel_pool_min = 20'000,
    .passive_chance = 900,
    .unknown_symbol_chance = 0,
    .zero_quantity_chance = 0,
    .bad_price_chance = 0,
    .reused_id_chance = 0,
};

// A narrow price range and few cancels, so most orders cross and the engine mostly trades.
inline constexpr GeneratorConfig aggressive{
    .symbol_count = 8,
    .mid_price = 10'000,
    .price_range = 5,
    .max_quantity = 100,
    .type_weights{8, 4, 2, 4, 2},
    .cancel_chance = 100,
    .cancel_random_chance = 0,
    .unknown_symbol_chance = 0,
    .zero_quantity_chance = 0,
    .bad_price_chance = 0,
    .reused_id_chance = 0,
};

// Looks up a config by name: "realistic", "aggressive", or "test" for the defaults.
inline std::optional<GeneratorConfig> by_name(std::string_view name) {
    if (name == "realistic") {
        return realistic;
    }
    if (name == "aggressive") {
        return aggressive;
    }
    if (name == "test") {
        return GeneratorConfig{};
    }
    return std::nullopt;
}

} // namespace profiles

// Generates a random command stream. The same seed and config always produce the same stream,
// on any platform: std::mt19937_64 is fully specified, and the distributions are written here
// because the standard library ones are not.
//
// The generator never sees the engine's output, so it only guesses which orders are resting.
// Cancels mostly name GTC orders it sent, which may since have been filled.
class CommandGenerator {
  public:
    explicit CommandGenerator(std::uint64_t seed, GeneratorConfig config = {})
        : rng_(seed), config_(config) {}

    EngineCommand next() {
        // With too few GTC orders to name, or none and no random cancels, send a new order instead.
        if (chance(config_.cancel_chance) && gtc_ids_.size() >= config_.cancel_pool_min &&
            (config_.cancel_random_chance > 0 || !gtc_ids_.empty())) {
            return cancel();
        }
        return new_order();
    }

    const GeneratorConfig& config() const { return config_; }

  private:
    NewOrder new_order() {
        NewOrder order{
            .id = next_id_,
            .symbol = static_cast<SymbolId>(below(config_.symbol_count)),
            .side = chance(500) ? OrderSide::Buy : OrderSide::Sell,
            .type = order_type(),
            .qty = 1 + below(config_.max_quantity),
            .price = std::nullopt,
        };
        const bool market =
            order.type == OrderType::MarketIOC || order.type == OrderType::MarketFOK;

        if (next_id_ > 1 && chance(config_.reused_id_chance)) {
            order.id = 1 + below(next_id_ - 1);
        } else {
            ++next_id_;
        }
        if (chance(config_.unknown_symbol_chance)) {
            order.symbol = static_cast<SymbolId>(config_.symbol_count + below(3));
        }
        if (chance(config_.zero_quantity_chance)) {
            order.qty = 0;
        }
        if (chance(config_.bad_price_chance)) {
            // A market order with a price, or a limit order with no price or a price of 0.
            order.price = market        ? std::optional<Price>{limit_price()}
                          : chance(500) ? std::nullopt
                                        : std::optional<Price>{0};
        } else if (!market) {
            order.price = passive(order.type) ? passive_price(order.side) : limit_price();
        }

        if (order.type == OrderType::LimitGTC) {
            gtc_ids_.push_back(order.id);
        }
        return order;
    }

    CancelOrder cancel() {
        if (gtc_ids_.empty() || chance(config_.cancel_random_chance)) {
            // Any ID sent so far, or a few never sent.
            return CancelOrder{.id = 1 + below(next_id_ + 10)};
        }
        // Each GTC order is named at most once, so cancels don't pile up on filled orders.
        const std::size_t index = below(gtc_ids_.size());
        const OrderId id = gtc_ids_[index];
        gtc_ids_[index] = gtc_ids_.back();
        gtc_ids_.pop_back();
        return CancelOrder{.id = id};
    }

    OrderType order_type() {
        unsigned total = 0;
        for (const unsigned weight : config_.type_weights) {
            total += weight;
        }
        auto pick = below(total);
        for (std::size_t type = 0; type < config_.type_weights.size(); ++type) {
            if (pick < config_.type_weights[type]) {
                return static_cast<OrderType>(type);
            }
            pick -= config_.type_weights[type];
        }
        return OrderType::LimitGTC; // Unreachable while some weight is non-zero
    }

    Price limit_price() {
        return config_.mid_price - config_.price_range + below(2 * config_.price_range + 1);
    }

    // Draws only when passive_chance is set, so configs without it give the same stream as before.
    bool passive(OrderType type) {
        return type == OrderType::LimitGTC && config_.passive_chance > 0 &&
               chance(config_.passive_chance);
    }

    // 1 to price_range ticks away from mid_price on the order's own side, so it never crosses
    // another passive order.
    Price passive_price(OrderSide side) {
        const Price offset = 1 + below(config_.price_range);
        return side == OrderSide::Buy ? config_.mid_price - offset : config_.mid_price + offset;
    }

    // Uniform in [0, bound). Rejects the top of the range that would bias the modulo.
    std::uint64_t below(std::uint64_t bound) {
        if (bound <= 1) {
            return 0;
        }
        const std::uint64_t limit = std::mt19937_64::max() - std::mt19937_64::max() % bound;
        std::uint64_t value = rng_();
        while (value >= limit) {
            value = rng_();
        }
        return value % bound;
    }

    bool chance(unsigned thousandths) { return below(1000) < thousandths; }

    std::mt19937_64 rng_;
    GeneratorConfig config_;
    OrderId next_id_ = 1;
    std::vector<OrderId> gtc_ids_; // GTC orders sent and not yet named by a cancel
};

} // namespace matching::workload
