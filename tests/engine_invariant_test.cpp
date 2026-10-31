#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "engines.hpp"
#include "matching/workload/command_generator.hpp"
#include "printers.hpp"

// Runs long random command streams through every engine listed in engines.hpp, and checks after
// every command that the event batch keeps the rules in documentation/semantics.md and
// documentation/interfaces.md. The checks only read event batches, so they hold for any engine.

using namespace matching;

namespace {

bool is_market(OrderType type) {
    return type == OrderType::MarketIOC || type == OrderType::MarketFOK;
}

bool is_fok(OrderType type) {
    return type == OrderType::LimitFOK || type == OrderType::MarketFOK;
}

OrderSide opposite(OrderSide side) {
    return side == OrderSide::Buy ? OrderSide::Sell : OrderSide::Buy;
}

// Whether an aggressor with `limit` may trade with an order resting at `price`.
bool crosses(OrderSide aggressor_side, std::optional<Price> limit, Price price) {
    if (!limit) {
        return true;
    }
    return aggressor_side == OrderSide::Buy ? price <= *limit : price >= *limit;
}

// How often each kind of outcome happened, so a test can check the stream exercised them all.
struct Coverage {
    std::array<std::size_t, 6> rejected{}; // Indexed by RejectReason
    std::size_t trades = 0;
    std::size_t rested = 0;
    std::size_t ioc_cancelled = 0;
    std::size_t fok_filled = 0;
    std::size_t fok_killed = 0;
    std::size_t cancelled = 0;
};

// The books as the events describe them. Each command's batch is checked against the model, then
// applied to it, so a wrong event is reported at the command that caused it.
class ShadowModel {
  public:
    explicit ShadowModel(std::size_t symbol_count) : books_(symbol_count) {}

    // Checks one command's batch with gtest assertions, and applies it to the model.
    void check(const EngineCommand& command, const EventBatch& batch) {
        EXPECT_EQ(batch.sequence, last_sequence_ + 1) << "sequence must increase by 1";
        last_sequence_ = batch.sequence;

        if (const auto* order = std::get_if<NewOrder>(&command)) {
            check_new(*order, batch);
        } else {
            check_cancel(std::get<CancelOrder>(command), batch);
        }
        check_not_crossed();
    }

    const Coverage& coverage() const { return coverage_; }

  private:
    struct RestingOrder {
        SymbolId symbol;
        OrderSide side;
        Price price;
        Quantity remaining;
    };

    // Price levels of one side, each a queue of order IDs in time priority.
    using Levels = std::map<Price, std::list<OrderId>>;

    struct Book {
        Levels bids;
        Levels asks;
    };

    Levels& levels(SymbolId symbol, OrderSide side) {
        return side == OrderSide::Buy ? books_[symbol].bids : books_[symbol].asks;
    }

    // The best level of a side: the highest bid or the lowest ask. end() when the side is empty.
    static Levels::iterator best(Levels& levels, OrderSide side) {
        if (levels.empty()) {
            return levels.end();
        }
        return side == OrderSide::Buy ? std::prev(levels.end()) : levels.begin();
    }

    // The first reason semantics.md gives for rejecting `order`, if any.
    std::optional<RejectReason> rejection(const NewOrder& order) {
        if (accepted_.contains(order.id)) {
            return RejectReason::DuplicateOrderId;
        }
        if (order.symbol >= books_.size()) {
            return RejectReason::UnknownSymbol;
        }
        if (order.qty == 0) {
            return RejectReason::InvalidQuantity;
        }
        if (is_market(order.type) ? order.price.has_value() : order.price.value_or(0) == 0) {
            return RejectReason::InvalidPrice;
        }
        if (is_market(order.type) && levels(order.symbol, opposite(order.side)).empty()) {
            return RejectReason::NoLiquidity;
        }
        return std::nullopt;
    }

    // Resting quantity that `order` could trade with.
    Quantity crossing_quantity(const NewOrder& order) {
        Quantity total = 0;
        for (const auto& [price, queue] : levels(order.symbol, opposite(order.side))) {
            if (crosses(order.side, order.price, price)) {
                for (const OrderId id : queue) {
                    total += resting_.at(id).remaining;
                }
            }
        }
        return total;
    }

    void check_new(const NewOrder& order, const EventBatch& batch) {
        if (const auto reason = rejection(order)) {
            ++coverage_.rejected[static_cast<std::size_t>(*reason)];
            EXPECT_EQ(batch, (EventBatch{batch.sequence, {OrderRejected{order.id, *reason}}}));
            return;
        }

        const auto& events = batch.events;
        ASSERT_FALSE(events.empty());
        EXPECT_EQ(events.front(), Event{OrderAccepted{order.id}});
        accepted_.insert(order.id);
        const Quantity crossing = crossing_quantity(order);

        // Trades, each with the order at the front of the best opposite level.
        Quantity unfilled = order.qty;
        std::size_t next = 1;
        for (; next < events.size(); ++next) {
            const auto* trade = std::get_if<Trade>(&events[next]);
            if (trade == nullptr) {
                break;
            }
            ASSERT_GT(unfilled, 0u) << "trade after the order was filled";
            apply_trade(order, *trade, unfilled);
            if (::testing::Test::HasFailure()) {
                return;
            }
            unfilled -= trade->quantity;
        }
        const Quantity traded = order.qty - unfilled;

        // At most one final event, which accounts for the unfilled quantity.
        Quantity rested = 0;
        Quantity cancelled = 0;
        if (next < events.size()) {
            ASSERT_EQ(next + 1, events.size()) << "events after the final event";
            if (const auto* rest = std::get_if<OrderRested>(&events[next])) {
                ASSERT_EQ(order.type, OrderType::LimitGTC) << "only GTC orders rest";
                ASSERT_EQ(*rest, (OrderRested{order.id, *order.price, unfilled}));
                add_resting(order, unfilled);
                rested = rest->remaining;
            } else if (const auto* cancel = std::get_if<OrderCancelled>(&events[next])) {
                ASSERT_NE(order.type, OrderType::LimitGTC) << "a GTC remainder rests";
                ASSERT_EQ(*cancel, (OrderCancelled{order.id, unfilled}));
                cancelled = cancel->remaining;
            } else {
                FAIL() << "unexpected final event " << ::testing::PrintToString(events[next]);
            }
        }
        EXPECT_EQ(traded + rested + cancelled, order.qty) << "quantity must balance";

        if (is_fok(order.type)) {
            if (crossing >= order.qty) {
                EXPECT_EQ(traded, order.qty) << "a FOK with enough crossing quantity fills";
                ++coverage_.fok_filled;
            } else {
                EXPECT_EQ(traded, 0u) << "a killed FOK must not trade";
                ++coverage_.fok_killed;
            }
        } else if (unfilled > 0) {
            EXPECT_EQ(crossing_quantity(order), 0u) << "matching stopped while orders still cross";
            ++(order.type == OrderType::LimitGTC ? coverage_.rested : coverage_.ioc_cancelled);
        }
    }

    // Checks a trade against the front of the best opposite level, and takes its quantity.
    void apply_trade(const NewOrder& order, const Trade& trade, Quantity unfilled) {
        const OrderSide resting_side = opposite(order.side);
        Levels& side = levels(order.symbol, resting_side);
        const auto level = best(side, resting_side);
        ASSERT_NE(level, side.end()) << "trade against an empty side";
        ASSERT_TRUE(crosses(order.side, order.price, level->first))
            << "trade breaks the aggressor's limit";

        const OrderId resting_id = level->second.front();
        RestingOrder& resting = resting_.at(resting_id);
        // Price-time priority, at the resting price, for as much as both orders allow.
        ASSERT_EQ(trade, (Trade{order.id, resting_id, order.side, level->first,
                                std::min(unfilled, resting.remaining)}));
        ++coverage_.trades;

        resting.remaining -= trade.quantity;
        if (resting.remaining == 0) {
            resting_.erase(resting_id);
            level->second.pop_front();
            if (level->second.empty()) {
                side.erase(level);
            }
        }
    }

    void add_resting(const NewOrder& order, Quantity remaining) {
        resting_.emplace(order.id, RestingOrder{order.symbol, order.side, *order.price, remaining});
        levels(order.symbol, order.side)[*order.price].push_back(order.id);
    }

    // A cancel succeeds exactly when the order is resting, and removes all of its remainder.
    void check_cancel(const CancelOrder& cancel, const EventBatch& batch) {
        const auto it = resting_.find(cancel.id);
        if (it == resting_.end()) {
            EXPECT_EQ(batch, (EventBatch{batch.sequence,
                                         {CancelRejected{cancel.id, RejectReason::UnknownOrder}}}));
            ++coverage_.rejected[static_cast<std::size_t>(RejectReason::UnknownOrder)];
            return;
        }

        const RestingOrder& order = it->second;
        EXPECT_EQ(batch,
                  (EventBatch{batch.sequence, {OrderCancelled{cancel.id, order.remaining}}}));
        ++coverage_.cancelled;

        Levels& side = levels(order.symbol, order.side);
        const auto level = side.find(order.price);
        level->second.remove(cancel.id);
        if (level->second.empty()) {
            side.erase(level);
        }
        resting_.erase(it);
    }

    void check_not_crossed() {
        for (std::size_t symbol = 0; symbol < books_.size(); ++symbol) {
            Book& book = books_[symbol];
            if (!book.bids.empty() && !book.asks.empty()) {
                EXPECT_LT(best(book.bids, OrderSide::Buy)->first,
                          best(book.asks, OrderSide::Sell)->first)
                    << "book for symbol " << symbol << " is crossed";
            }
        }
    }

    std::vector<Book> books_; // Indexed by SymbolId
    std::unordered_set<OrderId> accepted_;
    std::unordered_map<OrderId, RestingOrder> resting_;
    Sequence last_sequence_ = 0;
    Coverage coverage_;
};

constexpr std::size_t CommandsPerSeed = 20'000;
constexpr std::array<std::uint64_t, 8> Seeds{1, 2, 3, 4, 5, 6, 7, 8};

// The parameters are the engine's index in test::AnyEngine and the generator's seed.
class EngineInvariantTest
    : public ::testing::TestWithParam<std::tuple<std::size_t, std::uint64_t>> {};

} // namespace

INSTANTIATE_TEST_SUITE_P(
    AllEngines, EngineInvariantTest,
    ::testing::Combine(::testing::Range<std::size_t>(0, test::engine_names.size()),
                       ::testing::ValuesIn(Seeds)),
    [](const ::testing::TestParamInfo<EngineInvariantTest::ParamType>& info) {
        return std::string(test::engine_names[std::get<0>(info.param)]) + "_seed" +
               std::to_string(std::get<1>(info.param));
    });

TEST_P(EngineInvariantTest, RandomStream) {
    const auto [engine_index, seed] = GetParam();
    // Rerun one seed with --gtest_filter='*_seed<seed>'.
    SCOPED_TRACE("seed " + std::to_string(seed));

    workload::CommandGenerator generator(seed);
    const std::size_t symbol_count = generator.config().symbol_count;
    test::AnyEngine engine = test::make_engine(engine_index, symbol_count);
    ShadowModel model(symbol_count);

    for (std::size_t i = 1; i <= CommandsPerSeed; ++i) {
        const EngineCommand command = generator.next();
        const EventBatch batch = test::process(engine, command);
        model.check(command, batch);
        if (HasFailure()) {
            FAIL() << "first failure at command " << i << " of seed " << seed << ": "
                   << ::testing::PrintToString(command) << " gave "
                   << ::testing::PrintToString(batch.events);
        }
    }

    // Guards against a generator change that stops exercising part of the contract.
    const Coverage& coverage = model.coverage();
    for (std::size_t reason = 0; reason < coverage.rejected.size(); ++reason) {
        EXPECT_GT(coverage.rejected[reason], 0u)
            << "no " << to_string(static_cast<RejectReason>(reason)) << " rejection";
    }
    EXPECT_GT(coverage.trades, 0u);
    EXPECT_GT(coverage.rested, 0u);
    EXPECT_GT(coverage.ioc_cancelled, 0u);
    EXPECT_GT(coverage.fok_filled, 0u);
    EXPECT_GT(coverage.fok_killed, 0u);
    EXPECT_GT(coverage.cancelled, 0u);
}

TEST(CommandGeneratorTest, SameSeedSameStream) {
    workload::CommandGenerator first(42);
    workload::CommandGenerator second(42);
    for (int i = 0; i < 1'000; ++i) {
        ASSERT_EQ(first.next(), second.next()) << "command " << i;
    }
}

TEST(CommandGeneratorTest, DifferentSeedsDiffer) {
    workload::CommandGenerator first(1);
    workload::CommandGenerator second(2);
    std::vector<EngineCommand> a;
    std::vector<EngineCommand> b;
    for (int i = 0; i < 100; ++i) {
        a.push_back(first.next());
        b.push_back(second.next());
    }
    EXPECT_NE(a, b);
}

// The benchmark profiles must send only well-formed new orders with unique IDs, and cancels that
// name only IDs already sent.
TEST(CommandGeneratorTest, BenchmarkProfilesSendWellFormedCommands) {
    for (const workload::GeneratorConfig& config :
         {workload::profiles::realistic, workload::profiles::aggressive}) {
        workload::CommandGenerator generator(7, config);
        std::unordered_set<OrderId> sent;
        for (int i = 0; i < 20'000; ++i) {
            const EngineCommand command = generator.next();
            if (const auto* order = std::get_if<NewOrder>(&command)) {
                ASSERT_TRUE(sent.insert(order->id).second) << "reused ID " << order->id;
                ASSERT_LT(order->symbol, config.symbol_count);
                ASSERT_GT(order->qty, 0u);
                ASSERT_EQ(order->price.has_value(), !is_market(order->type));
                if (order->price) {
                    ASSERT_GT(*order->price, 0u);
                }
            } else {
                ASSERT_TRUE(sent.contains(std::get<CancelOrder>(command).id));
            }
        }
    }
}

TEST(CommandGeneratorTest, ProfilesByName) {
    EXPECT_EQ(workload::profiles::by_name("realistic"), workload::profiles::realistic);
    EXPECT_EQ(workload::profiles::by_name("aggressive"), workload::profiles::aggressive);
    EXPECT_EQ(workload::profiles::by_name("test"), workload::GeneratorConfig{});
    EXPECT_EQ(workload::profiles::by_name("unknown"), std::nullopt);
}
