#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include "engines.hpp"
#include "printers.hpp"

// Every test follows documentation/semantics.md and documentation/interfaces.md, and runs
// against every engine listed in engines.hpp with both a buy and a sell aggressor.

using namespace matching;

namespace {

using Events = std::vector<Event>;

// The engines under test are built with two symbols, so any other ID is unknown.
constexpr SymbolId AAPL = 0;
constexpr SymbolId MSFT = 1;
constexpr SymbolId UnknownSymbol = 2;
constexpr std::size_t SymbolCount = 2;

NewOrder order(OrderId id, OrderSide side, OrderType type, Quantity qty, std::optional<Price> price,
               SymbolId symbol = AAPL) {
    return NewOrder{
        .id = id,
        .symbol = symbol,
        .side = side,
        .type = type,
        .qty = qty,
        .price = price,
    };
}

NewOrder gtc(OrderId id, OrderSide side, Quantity qty, Price price, SymbolId symbol = AAPL) {
    return order(id, side, OrderType::LimitGTC, qty, price, symbol);
}

// Each test is written for a buy aggressor against resting sells, and runs once per side. For a
// sell aggressor the sides swap and prices mirror around Pivot, so a better price for the
// aggressor stays better.
constexpr Price Pivot = 1000;

using Param = std::tuple<std::size_t, OrderSide>; // Engine index in test::AnyEngine, aggressor side

class EngineTest : public ::testing::TestWithParam<Param> {
  protected:
    EventBatch process(const EngineCommand& command) {
        return std::visit([&](auto& engine) { return engine.process(command); }, engine_);
    }

    Events run(const EngineCommand& command) { return process(command).events; }

    // The price `p` as seen from the aggressor's side.
    Price px(Price p) const { return side == OrderSide::Buy ? p : Pivot - p; }

    const OrderSide side = std::get<1>(GetParam());
    const OrderSide opposite = side == OrderSide::Buy ? OrderSide::Sell : OrderSide::Buy;
    test::AnyEngine engine_ = test::make_engine(std::get<0>(GetParam()), SymbolCount);
};

} // namespace

INSTANTIATE_TEST_SUITE_P(
    AllEngines, EngineTest,
    ::testing::Combine(::testing::Range<std::size_t>(0, test::engine_names.size()),
                       ::testing::Values(OrderSide::Buy, OrderSide::Sell)),
    [](const ::testing::TestParamInfo<Param>& info) {
        return std::string(test::engine_names[std::get<0>(info.param)]) + "_" +
               to_string(std::get<1>(info.param));
    });

// --- Sequencing ---

TEST_P(EngineTest, SequenceStartsAtOneAndIncrements) {
    const auto first = process(gtc(1, side, 10, px(100)));
    const auto second = process(CancelOrder{.id = 1});

    EXPECT_EQ(first.sequence, 1u);
    EXPECT_EQ(second.sequence, 2u);
}

TEST_P(EngineTest, RejectedCommandUsesSequence) {
    const auto rejected = process(gtc(1, side, 10, px(100), UnknownSymbol));
    const auto accepted = process(gtc(2, side, 10, px(100)));

    EXPECT_EQ(rejected.sequence, 1u);
    EXPECT_EQ(rejected.events, (Events{OrderRejected{1, RejectReason::UnknownSymbol}}));
    EXPECT_EQ(accepted.sequence, 2u);
}

// --- Validation ---

TEST_P(EngineTest, UnknownSymbol) {
    EXPECT_EQ(run(gtc(1, side, 10, px(100), UnknownSymbol)),
              (Events{OrderRejected{1, RejectReason::UnknownSymbol}}));
}

TEST_P(EngineTest, ZeroQuantity) {
    EXPECT_EQ(run(gtc(1, side, 0, px(100))),
              (Events{OrderRejected{1, RejectReason::InvalidQuantity}}));
}

TEST_P(EngineTest, LimitWithoutPrice) {
    EXPECT_EQ(run(order(1, side, OrderType::LimitGTC, 10, std::nullopt)),
              (Events{OrderRejected{1, RejectReason::InvalidPrice}}));
}

TEST_P(EngineTest, LimitWithZeroPrice) {
    EXPECT_EQ(run(gtc(1, side, 10, 0)), (Events{OrderRejected{1, RejectReason::InvalidPrice}}));
}

TEST_P(EngineTest, MarketWithPrice) {
    EXPECT_EQ(run(order(1, side, OrderType::MarketIOC, 10, px(100))),
              (Events{OrderRejected{1, RejectReason::InvalidPrice}}));
}

TEST_P(EngineTest, MarketIOCOnEmptyBook) {
    EXPECT_EQ(run(order(1, side, OrderType::MarketIOC, 10, std::nullopt)),
              (Events{OrderRejected{1, RejectReason::NoLiquidity}}));
}

TEST_P(EngineTest, MarketFOKOnEmptyBook) {
    EXPECT_EQ(run(order(1, side, OrderType::MarketFOK, 10, std::nullopt)),
              (Events{OrderRejected{1, RejectReason::NoLiquidity}}));
}

TEST_P(EngineTest, NoLiquidityLooksAtOppositeSideOnly) {
    run(gtc(1, side, 10, px(100)));

    EXPECT_EQ(run(order(2, side, OrderType::MarketIOC, 10, std::nullopt)),
              (Events{OrderRejected{2, RejectReason::NoLiquidity}}));
}

TEST_P(EngineTest, DuplicateIdOfRestingOrder) {
    run(gtc(1, side, 10, px(100)));

    EXPECT_EQ(run(gtc(1, side, 10, px(100))),
              (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, DuplicateIdOfFilledOrder) {
    run(gtc(1, opposite, 10, px(100)));
    run(gtc(2, side, 10, px(100)));

    EXPECT_EQ(run(gtc(1, side, 10, px(100))),
              (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, DuplicateIdOfCancelledOrder) {
    run(gtc(1, side, 10, px(100)));
    run(CancelOrder{.id = 1});

    EXPECT_EQ(run(gtc(1, side, 10, px(100))),
              (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, DuplicateIdAcrossSymbols) {
    run(gtc(1, side, 10, px(100), AAPL));

    EXPECT_EQ(run(gtc(1, side, 10, px(100), MSFT)),
              (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, RejectedIdCanBeReused) {
    run(gtc(1, side, 0, px(100)));

    EXPECT_EQ(run(gtc(1, side, 10, px(100))),
              (Events{OrderAccepted{1}, OrderRested{1, px(100), 10}}));
}

TEST_P(EngineTest, RejectedOrderLeavesBookUntouched) {
    run(gtc(1, opposite, 10, px(100)));

    EXPECT_EQ(run(gtc(1, side, 10, px(100))),
              (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
    EXPECT_EQ(run(gtc(2, side, 10, px(100))),
              (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 10}}));
}

// Validation order (the first failing check wins).

TEST_P(EngineTest, DuplicateBeatsUnknownSymbol) {
    run(gtc(1, side, 10, px(100)));

    EXPECT_EQ(run(gtc(1, side, 10, px(100), UnknownSymbol)),
              (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, UnknownSymbolBeatsQuantity) {
    EXPECT_EQ(run(gtc(1, side, 0, px(100), UnknownSymbol)),
              (Events{OrderRejected{1, RejectReason::UnknownSymbol}}));
}

TEST_P(EngineTest, QuantityBeatsPrice) {
    EXPECT_EQ(run(gtc(1, side, 0, 0)), (Events{OrderRejected{1, RejectReason::InvalidQuantity}}));
}

TEST_P(EngineTest, PriceBeatsNoLiquidity) {
    EXPECT_EQ(run(order(1, side, OrderType::MarketIOC, 10, px(100))),
              (Events{OrderRejected{1, RejectReason::InvalidPrice}}));
}

// --- Resting ---

TEST_P(EngineTest, GTCRestsOnEmptyBook) {
    EXPECT_EQ(run(gtc(1, side, 10, px(100))),
              (Events{OrderAccepted{1}, OrderRested{1, px(100), 10}}));
}

TEST_P(EngineTest, NonCrossingGTCRests) {
    run(gtc(1, opposite, 10, px(101)));

    EXPECT_EQ(run(gtc(2, side, 10, px(100))),
              (Events{OrderAccepted{2}, OrderRested{2, px(100), 10}}));
}

TEST_P(EngineTest, SymbolsHaveSeparateBooks) {
    run(gtc(1, opposite, 10, px(100), AAPL));

    EXPECT_EQ(run(gtc(2, side, 10, px(100), MSFT)),
              (Events{OrderAccepted{2}, OrderRested{2, px(100), 10}}));
}

// --- Matching ---

TEST_P(EngineTest, ExactFill) {
    run(gtc(1, opposite, 10, px(100)));

    EXPECT_EQ(run(gtc(2, side, 10, px(100))),
              (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 10}}));
}

TEST_P(EngineTest, FilledRestingOrderLeavesBook) {
    run(gtc(1, opposite, 10, px(100)));
    run(gtc(2, side, 10, px(100)));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{CancelRejected{1, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, TradesAtRestingPrice) {
    run(gtc(1, opposite, 10, px(100)));

    EXPECT_EQ(run(gtc(2, side, 10, px(105))),
              (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 10}}));
}

TEST_P(EngineTest, IncomingRemainderRests) {
    run(gtc(1, opposite, 4, px(100)));

    EXPECT_EQ(
        run(gtc(2, side, 10, px(100))),
        (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 4}, OrderRested{2, px(100), 6}}));
}

TEST_P(EngineTest, RestingRemainderStays) {
    run(gtc(1, opposite, 10, px(100)));

    EXPECT_EQ(run(gtc(2, side, 4, px(100))),
              (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 4}}));
    EXPECT_EQ(run(gtc(3, side, 6, px(100))),
              (Events{OrderAccepted{3}, Trade{3, 1, side, px(100), 6}}));
    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{CancelRejected{1, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, SweepsBestPriceFirst) {
    run(gtc(1, opposite, 5, px(101)));
    run(gtc(2, opposite, 5, px(100)));

    EXPECT_EQ(
        run(gtc(3, side, 10, px(101))),
        (Events{OrderAccepted{3}, Trade{3, 2, side, px(100), 5}, Trade{3, 1, side, px(101), 5}}));
}

TEST_P(EngineTest, StopsAtLimitPrice) {
    run(gtc(1, opposite, 5, px(100)));
    run(gtc(2, opposite, 5, px(102)));

    EXPECT_EQ(
        run(gtc(3, side, 10, px(101))),
        (Events{OrderAccepted{3}, Trade{3, 1, side, px(100), 5}, OrderRested{3, px(101), 5}}));
    EXPECT_EQ(run(CancelOrder{.id = 2}), (Events{OrderCancelled{2, 5}}));
}

TEST_P(EngineTest, TimePriorityWithinLevel) {
    run(gtc(1, opposite, 5, px(100)));
    run(gtc(2, opposite, 5, px(100)));

    EXPECT_EQ(run(gtc(3, side, 5, px(100))),
              (Events{OrderAccepted{3}, Trade{3, 1, side, px(100), 5}}));
}

TEST_P(EngineTest, PartialFillKeepsQueuePosition) {
    run(gtc(1, opposite, 10, px(100)));
    run(gtc(2, opposite, 10, px(100)));

    EXPECT_EQ(run(gtc(3, side, 4, px(100))),
              (Events{OrderAccepted{3}, Trade{3, 1, side, px(100), 4}}));
    EXPECT_EQ(run(gtc(4, side, 4, px(100))),
              (Events{OrderAccepted{4}, Trade{4, 1, side, px(100), 4}}));
}

TEST_P(EngineTest, OneTradePerRestingOrder) {
    run(gtc(1, opposite, 3, px(100)));
    run(gtc(2, opposite, 3, px(100)));

    EXPECT_EQ(
        run(gtc(3, side, 6, px(100))),
        (Events{OrderAccepted{3}, Trade{3, 1, side, px(100), 3}, Trade{3, 2, side, px(100), 3}}));
}

// --- LimitIOC ---

TEST_P(EngineTest, IOCFullFill) {
    run(gtc(1, opposite, 10, px(100)));

    EXPECT_EQ(run(order(2, side, OrderType::LimitIOC, 10, px(100))),
              (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 10}}));
}

TEST_P(EngineTest, IOCRemainderCancelled) {
    run(gtc(1, opposite, 4, px(100)));

    EXPECT_EQ(run(order(2, side, OrderType::LimitIOC, 10, px(100))),
              (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 4}, OrderCancelled{2, 6}}));
}

TEST_P(EngineTest, IOCNoCross) {
    run(gtc(1, opposite, 10, px(101)));

    EXPECT_EQ(run(order(2, side, OrderType::LimitIOC, 10, px(100))),
              (Events{OrderAccepted{2}, OrderCancelled{2, 10}}));
}

TEST_P(EngineTest, IOCNeverRests) {
    run(gtc(1, opposite, 4, px(100)));
    run(order(2, side, OrderType::LimitIOC, 10, px(100)));

    EXPECT_EQ(run(CancelOrder{.id = 2}), (Events{CancelRejected{2, RejectReason::UnknownOrder}}));
}

// --- MarketIOC ---

TEST_P(EngineTest, MarketIOCIgnoresPrice) {
    run(gtc(1, opposite, 5, px(100)));
    run(gtc(2, opposite, 5, px(500)));

    EXPECT_EQ(
        run(order(3, side, OrderType::MarketIOC, 10, std::nullopt)),
        (Events{OrderAccepted{3}, Trade{3, 1, side, px(100), 5}, Trade{3, 2, side, px(500), 5}}));
}

TEST_P(EngineTest, MarketIOCRemainderCancelled) {
    run(gtc(1, opposite, 4, px(100)));

    EXPECT_EQ(run(order(2, side, OrderType::MarketIOC, 10, std::nullopt)),
              (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 4}, OrderCancelled{2, 6}}));
}

// --- LimitFOK ---

TEST_P(EngineTest, FOKFills) {
    run(gtc(1, opposite, 10, px(100)));

    EXPECT_EQ(run(order(2, side, OrderType::LimitFOK, 10, px(100))),
              (Events{OrderAccepted{2}, Trade{2, 1, side, px(100), 10}}));
}

TEST_P(EngineTest, FOKFillsAcrossLevels) {
    run(gtc(1, opposite, 5, px(100)));
    run(gtc(2, opposite, 5, px(101)));

    EXPECT_EQ(
        run(order(3, side, OrderType::LimitFOK, 10, px(101))),
        (Events{OrderAccepted{3}, Trade{3, 1, side, px(100), 5}, Trade{3, 2, side, px(101), 5}}));
}

TEST_P(EngineTest, FOKKilledWhenTooSmall) {
    run(gtc(1, opposite, 4, px(100)));

    EXPECT_EQ(run(order(2, side, OrderType::LimitFOK, 10, px(100))),
              (Events{OrderAccepted{2}, OrderCancelled{2, 10}}));
    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 4}}));
}

TEST_P(EngineTest, FOKCountsOnlyCrossingQuantity) {
    run(gtc(1, opposite, 5, px(100)));
    run(gtc(2, opposite, 5, px(105)));

    EXPECT_EQ(run(order(3, side, OrderType::LimitFOK, 10, px(100))),
              (Events{OrderAccepted{3}, OrderCancelled{3, 10}}));
}

TEST_P(EngineTest, FOKNeverRests) {
    run(gtc(1, opposite, 4, px(100)));
    run(order(2, side, OrderType::LimitFOK, 10, px(100)));

    EXPECT_EQ(run(CancelOrder{.id = 2}), (Events{CancelRejected{2, RejectReason::UnknownOrder}}));
}

// --- MarketFOK ---

TEST_P(EngineTest, MarketFOKFillsAtAnyPrice) {
    run(gtc(1, opposite, 5, px(100)));
    run(gtc(2, opposite, 5, px(500)));

    EXPECT_EQ(
        run(order(3, side, OrderType::MarketFOK, 10, std::nullopt)),
        (Events{OrderAccepted{3}, Trade{3, 1, side, px(100), 5}, Trade{3, 2, side, px(500), 5}}));
}

TEST_P(EngineTest, MarketFOKKilledOnThinBook) {
    run(gtc(1, opposite, 4, px(100)));

    EXPECT_EQ(run(order(2, side, OrderType::MarketFOK, 10, std::nullopt)),
              (Events{OrderAccepted{2}, OrderCancelled{2, 10}}));
    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 4}}));
}

// --- Cancels ---

TEST_P(EngineTest, CancelResting) {
    run(gtc(1, side, 10, px(100)));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 10}}));
}

TEST_P(EngineTest, CancelledOrderLeavesBook) {
    run(gtc(1, side, 10, px(100)));
    run(CancelOrder{.id = 1});

    EXPECT_EQ(run(gtc(2, opposite, 10, px(100))),
              (Events{OrderAccepted{2}, OrderRested{2, px(100), 10}}));
}

TEST_P(EngineTest, CancelPartiallyFilled) {
    run(gtc(1, opposite, 10, px(100)));
    run(gtc(2, side, 4, px(100)));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 6}}));
}

TEST_P(EngineTest, CancelUnknownId) {
    EXPECT_EQ(run(CancelOrder{.id = 42}), (Events{CancelRejected{42, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, CancelTwice) {
    run(gtc(1, side, 10, px(100)));
    run(CancelOrder{.id = 1});

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{CancelRejected{1, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, CancelRejectedOrder) {
    run(gtc(1, side, 0, px(100)));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{CancelRejected{1, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, CancelOnOtherSymbol) {
    run(gtc(1, side, 10, px(100), MSFT));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 10}}));
}
