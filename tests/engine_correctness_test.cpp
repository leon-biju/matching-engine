#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "engines.hpp"

// Every test follows documentation/semantics.md and documentation/interfaces.md, and runs
// against every engine listed in engines.hpp.

namespace matching {

// Readable gtest output for failed event comparisons, found by ADL.
const char* to_string(OrderSide side) {
    return side == OrderSide::Buy ? "Buy" : "Sell";
}

const char* to_string(RejectReason reason) {
    switch (reason) {
    case RejectReason::DuplicateOrderId:
        return "DuplicateOrderId";
    case RejectReason::UnknownSymbol:
        return "UnknownSymbol";
    case RejectReason::InvalidQuantity:
        return "InvalidQuantity";
    case RejectReason::InvalidPrice:
        return "InvalidPrice";
    case RejectReason::NoLiquidity:
        return "NoLiquidity";
    case RejectReason::UnknownOrder:
        return "UnknownOrder";
    }
    return "?";
}

void PrintTo(const OrderAccepted& e, std::ostream* os) {
    *os << "Accepted(" << e.id << ")";
}

void PrintTo(const OrderRejected& e, std::ostream* os) {
    *os << "Rejected(" << e.id << ", " << to_string(e.reason) << ")";
}

void PrintTo(const Trade& e, std::ostream* os) {
    *os << "Trade(" << e.aggressor_id << "->" << e.resting_id << ", " << to_string(e.aggressor_side)
        << ", " << e.quantity << "@" << e.price << ")";
}

void PrintTo(const OrderRested& e, std::ostream* os) {
    *os << "Rested(" << e.id << ", " << e.price << ", " << e.remaining << ")";
}

void PrintTo(const OrderCancelled& e, std::ostream* os) {
    *os << "Cancelled(" << e.id << ", " << e.remaining << ")";
}

void PrintTo(const CancelRejected& e, std::ostream* os) {
    *os << "CancelRejected(" << e.id << ", " << to_string(e.reason) << ")";
}

} // namespace matching

using namespace matching;

namespace {

using Events = std::vector<Event>;

constexpr OrderSide Buy = OrderSide::Buy;
constexpr OrderSide Sell = OrderSide::Sell;

NewOrder order(OrderId id, OrderSide side, OrderType type, Quantity qty,
               std::optional<Price> price, std::string symbol = "AAPL") {
    return NewOrder{
        .id = id,
        .symbol = std::move(symbol),
        .side = side,
        .type = type,
        .qty = qty,
        .price = price,
    };
}

NewOrder gtc(OrderId id, OrderSide side, Quantity qty, Price price, std::string symbol = "AAPL") {
    return order(id, side, OrderType::LimitGTC, qty, price, std::move(symbol));
}

// The parameter is the engine's index in test::AnyEngine.
class EngineTest : public ::testing::TestWithParam<std::size_t> {
protected:
    EventBatch process(const EngineCommand& command) {
        return std::visit([&](auto& engine) { return engine.process(command); }, engine_);
    }

    Events run(const EngineCommand& command) { return process(command).events; }

    test::AnyEngine engine_ = test::make_engine(GetParam(), {"AAPL", "MSFT"});
};

} // namespace

INSTANTIATE_TEST_SUITE_P(AllEngines, EngineTest,
                         ::testing::Range<std::size_t>(0, test::engine_names.size()),
                         [](const ::testing::TestParamInfo<std::size_t>& info) {
                             return std::string(test::engine_names[info.param]);
                         });

// --- Sequencing ---

TEST_P(EngineTest, SequenceStartsAtOneAndIncrements) {
    const auto first = process(gtc(1, Buy, 10, 100));
    const auto second = process(CancelOrder{.id = 1});

    EXPECT_EQ(first.sequence, 1u);
    EXPECT_EQ(second.sequence, 2u);
}

TEST_P(EngineTest, RejectedCommandUsesSequence) {
    const auto rejected = process(gtc(1, Buy, 10, 100, "TSLA"));
    const auto accepted = process(gtc(2, Buy, 10, 100));

    EXPECT_EQ(rejected.sequence, 1u);
    EXPECT_EQ(rejected.events, (Events{OrderRejected{1, RejectReason::UnknownSymbol}}));
    EXPECT_EQ(accepted.sequence, 2u);
}

// --- Validation ---

TEST_P(EngineTest, UnknownSymbol) {
    EXPECT_EQ(run(gtc(1, Buy, 10, 100, "TSLA")),
              (Events{OrderRejected{1, RejectReason::UnknownSymbol}}));
}

TEST_P(EngineTest, SymbolIsCaseSensitive) {
    EXPECT_EQ(run(gtc(1, Buy, 10, 100, "aapl")),
              (Events{OrderRejected{1, RejectReason::UnknownSymbol}}));
}

TEST_P(EngineTest, ZeroQuantity) {
    EXPECT_EQ(run(gtc(1, Buy, 0, 100)), (Events{OrderRejected{1, RejectReason::InvalidQuantity}}));
}

TEST_P(EngineTest, LimitWithoutPrice) {
    EXPECT_EQ(run(order(1, Buy, OrderType::LimitGTC, 10, std::nullopt)),
              (Events{OrderRejected{1, RejectReason::InvalidPrice}}));
}

TEST_P(EngineTest, LimitWithZeroPrice) {
    EXPECT_EQ(run(gtc(1, Buy, 10, 0)), (Events{OrderRejected{1, RejectReason::InvalidPrice}}));
}

TEST_P(EngineTest, MarketWithPrice) {
    EXPECT_EQ(run(order(1, Buy, OrderType::MarketIOC, 10, 100)),
              (Events{OrderRejected{1, RejectReason::InvalidPrice}}));
}

TEST_P(EngineTest, MarketIOCOnEmptyBook) {
    EXPECT_EQ(run(order(1, Buy, OrderType::MarketIOC, 10, std::nullopt)),
              (Events{OrderRejected{1, RejectReason::NoLiquidity}}));
}

TEST_P(EngineTest, MarketFOKOnEmptyBook) {
    EXPECT_EQ(run(order(1, Buy, OrderType::MarketFOK, 10, std::nullopt)),
              (Events{OrderRejected{1, RejectReason::NoLiquidity}}));
}

TEST_P(EngineTest, NoLiquidityLooksAtOppositeSideOnly) {
    run(gtc(1, Buy, 10, 100));

    EXPECT_EQ(run(order(2, Buy, OrderType::MarketIOC, 10, std::nullopt)),
              (Events{OrderRejected{2, RejectReason::NoLiquidity}}));
}

TEST_P(EngineTest, DuplicateIdOfRestingOrder) {
    run(gtc(1, Buy, 10, 100));

    EXPECT_EQ(run(gtc(1, Buy, 10, 100)), (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, DuplicateIdOfFilledOrder) {
    run(gtc(1, Sell, 10, 100));
    run(gtc(2, Buy, 10, 100));

    EXPECT_EQ(run(gtc(1, Buy, 10, 100)), (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, DuplicateIdOfCancelledOrder) {
    run(gtc(1, Buy, 10, 100));
    run(CancelOrder{.id = 1});

    EXPECT_EQ(run(gtc(1, Buy, 10, 100)), (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, DuplicateIdAcrossSymbols) {
    run(gtc(1, Buy, 10, 100, "AAPL"));

    EXPECT_EQ(run(gtc(1, Buy, 10, 100, "MSFT")),
              (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, RejectedIdCanBeReused) {
    run(gtc(1, Buy, 0, 100));

    EXPECT_EQ(run(gtc(1, Buy, 10, 100)), (Events{OrderAccepted{1}, OrderRested{1, 100, 10}}));
}

TEST_P(EngineTest, RejectedOrderLeavesBookUntouched) {
    run(gtc(1, Sell, 10, 100));

    EXPECT_EQ(run(gtc(1, Buy, 10, 100)), (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
    EXPECT_EQ(run(gtc(2, Buy, 10, 100)), (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 10}}));
}

// Validation order (the first failing check wins).

TEST_P(EngineTest, DuplicateBeatsUnknownSymbol) {
    run(gtc(1, Buy, 10, 100));

    EXPECT_EQ(run(gtc(1, Buy, 10, 100, "TSLA")),
              (Events{OrderRejected{1, RejectReason::DuplicateOrderId}}));
}

TEST_P(EngineTest, UnknownSymbolBeatsQuantity) {
    EXPECT_EQ(run(gtc(1, Buy, 0, 100, "TSLA")),
              (Events{OrderRejected{1, RejectReason::UnknownSymbol}}));
}

TEST_P(EngineTest, QuantityBeatsPrice) {
    EXPECT_EQ(run(gtc(1, Buy, 0, 0)), (Events{OrderRejected{1, RejectReason::InvalidQuantity}}));
}

TEST_P(EngineTest, PriceBeatsNoLiquidity) {
    EXPECT_EQ(run(order(1, Buy, OrderType::MarketIOC, 10, 100)),
              (Events{OrderRejected{1, RejectReason::InvalidPrice}}));
}

// --- Resting ---

TEST_P(EngineTest, GTCRestsOnEmptyBook) {
    EXPECT_EQ(run(gtc(1, Buy, 10, 100)), (Events{OrderAccepted{1}, OrderRested{1, 100, 10}}));
}

TEST_P(EngineTest, NonCrossingGTCRests) {
    run(gtc(1, Sell, 10, 101));

    EXPECT_EQ(run(gtc(2, Buy, 10, 100)), (Events{OrderAccepted{2}, OrderRested{2, 100, 10}}));
}

TEST_P(EngineTest, SymbolsHaveSeparateBooks) {
    run(gtc(1, Sell, 10, 100, "AAPL"));

    EXPECT_EQ(run(gtc(2, Buy, 10, 100, "MSFT")),
              (Events{OrderAccepted{2}, OrderRested{2, 100, 10}}));
}

// --- Matching ---

TEST_P(EngineTest, ExactFill) {
    run(gtc(1, Sell, 10, 100));

    EXPECT_EQ(run(gtc(2, Buy, 10, 100)), (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 10}}));
}

TEST_P(EngineTest, FilledRestingOrderLeavesBook) {
    run(gtc(1, Sell, 10, 100));
    run(gtc(2, Buy, 10, 100));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{CancelRejected{1, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, TradesAtRestingPriceForBuy) {
    run(gtc(1, Sell, 10, 100));

    EXPECT_EQ(run(gtc(2, Buy, 10, 105)), (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 10}}));
}

TEST_P(EngineTest, TradesAtRestingPriceForSell) {
    run(gtc(1, Buy, 10, 100));

    EXPECT_EQ(run(gtc(2, Sell, 10, 95)), (Events{OrderAccepted{2}, Trade{2, 1, Sell, 100, 10}}));
}

TEST_P(EngineTest, IncomingRemainderRests) {
    run(gtc(1, Sell, 4, 100));

    EXPECT_EQ(run(gtc(2, Buy, 10, 100)),
              (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 4}, OrderRested{2, 100, 6}}));
}

TEST_P(EngineTest, RestingRemainderStays) {
    run(gtc(1, Sell, 10, 100));

    EXPECT_EQ(run(gtc(2, Buy, 4, 100)), (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 4}}));
    EXPECT_EQ(run(gtc(3, Buy, 6, 100)), (Events{OrderAccepted{3}, Trade{3, 1, Buy, 100, 6}}));
    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{CancelRejected{1, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, SweepsBestPriceFirst) {
    run(gtc(1, Sell, 5, 101));
    run(gtc(2, Sell, 5, 100));

    EXPECT_EQ(run(gtc(3, Buy, 10, 101)),
              (Events{OrderAccepted{3}, Trade{3, 2, Buy, 100, 5}, Trade{3, 1, Buy, 101, 5}}));
}

TEST_P(EngineTest, SellSweepsHighestBidFirst) {
    run(gtc(1, Buy, 5, 100));
    run(gtc(2, Buy, 5, 101));

    EXPECT_EQ(run(gtc(3, Sell, 10, 100)),
              (Events{OrderAccepted{3}, Trade{3, 2, Sell, 101, 5}, Trade{3, 1, Sell, 100, 5}}));
}

TEST_P(EngineTest, StopsAtLimitPrice) {
    run(gtc(1, Sell, 5, 100));
    run(gtc(2, Sell, 5, 102));

    EXPECT_EQ(run(gtc(3, Buy, 10, 101)),
              (Events{OrderAccepted{3}, Trade{3, 1, Buy, 100, 5}, OrderRested{3, 101, 5}}));
    EXPECT_EQ(run(CancelOrder{.id = 2}), (Events{OrderCancelled{2, 5}}));
}

TEST_P(EngineTest, TimePriorityWithinLevel) {
    run(gtc(1, Sell, 5, 100));
    run(gtc(2, Sell, 5, 100));

    EXPECT_EQ(run(gtc(3, Buy, 5, 100)), (Events{OrderAccepted{3}, Trade{3, 1, Buy, 100, 5}}));
}

TEST_P(EngineTest, PartialFillKeepsQueuePosition) {
    run(gtc(1, Sell, 10, 100));
    run(gtc(2, Sell, 10, 100));

    EXPECT_EQ(run(gtc(3, Buy, 4, 100)), (Events{OrderAccepted{3}, Trade{3, 1, Buy, 100, 4}}));
    EXPECT_EQ(run(gtc(4, Buy, 4, 100)), (Events{OrderAccepted{4}, Trade{4, 1, Buy, 100, 4}}));
}

TEST_P(EngineTest, OneTradePerRestingOrder) {
    run(gtc(1, Sell, 3, 100));
    run(gtc(2, Sell, 3, 100));

    EXPECT_EQ(run(gtc(3, Buy, 6, 100)),
              (Events{OrderAccepted{3}, Trade{3, 1, Buy, 100, 3}, Trade{3, 2, Buy, 100, 3}}));
}

// --- LimitIOC ---

TEST_P(EngineTest, IOCFullFill) {
    run(gtc(1, Sell, 10, 100));

    EXPECT_EQ(run(order(2, Buy, OrderType::LimitIOC, 10, 100)),
              (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 10}}));
}

TEST_P(EngineTest, IOCRemainderCancelled) {
    run(gtc(1, Sell, 4, 100));

    EXPECT_EQ(run(order(2, Buy, OrderType::LimitIOC, 10, 100)),
              (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 4}, OrderCancelled{2, 6}}));
}

TEST_P(EngineTest, IOCNoCross) {
    run(gtc(1, Sell, 10, 101));

    EXPECT_EQ(run(order(2, Buy, OrderType::LimitIOC, 10, 100)),
              (Events{OrderAccepted{2}, OrderCancelled{2, 10}}));
}

TEST_P(EngineTest, IOCNeverRests) {
    run(gtc(1, Sell, 4, 100));
    run(order(2, Buy, OrderType::LimitIOC, 10, 100));

    EXPECT_EQ(run(CancelOrder{.id = 2}), (Events{CancelRejected{2, RejectReason::UnknownOrder}}));
}

// --- MarketIOC ---

TEST_P(EngineTest, MarketIOCIgnoresPrice) {
    run(gtc(1, Sell, 5, 100));
    run(gtc(2, Sell, 5, 500));

    EXPECT_EQ(run(order(3, Buy, OrderType::MarketIOC, 10, std::nullopt)),
              (Events{OrderAccepted{3}, Trade{3, 1, Buy, 100, 5}, Trade{3, 2, Buy, 500, 5}}));
}

TEST_P(EngineTest, MarketIOCRemainderCancelled) {
    run(gtc(1, Sell, 4, 100));

    EXPECT_EQ(run(order(2, Buy, OrderType::MarketIOC, 10, std::nullopt)),
              (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 4}, OrderCancelled{2, 6}}));
}

// --- LimitFOK ---

TEST_P(EngineTest, FOKFills) {
    run(gtc(1, Sell, 10, 100));

    EXPECT_EQ(run(order(2, Buy, OrderType::LimitFOK, 10, 100)),
              (Events{OrderAccepted{2}, Trade{2, 1, Buy, 100, 10}}));
}

TEST_P(EngineTest, FOKFillsAcrossLevels) {
    run(gtc(1, Sell, 5, 100));
    run(gtc(2, Sell, 5, 101));

    EXPECT_EQ(run(order(3, Buy, OrderType::LimitFOK, 10, 101)),
              (Events{OrderAccepted{3}, Trade{3, 1, Buy, 100, 5}, Trade{3, 2, Buy, 101, 5}}));
}

TEST_P(EngineTest, FOKKilledWhenTooSmall) {
    run(gtc(1, Sell, 4, 100));

    EXPECT_EQ(run(order(2, Buy, OrderType::LimitFOK, 10, 100)),
              (Events{OrderAccepted{2}, OrderCancelled{2, 10}}));
    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 4}}));
}

TEST_P(EngineTest, FOKCountsOnlyCrossingQuantity) {
    run(gtc(1, Sell, 5, 100));
    run(gtc(2, Sell, 5, 105));

    EXPECT_EQ(run(order(3, Buy, OrderType::LimitFOK, 10, 100)),
              (Events{OrderAccepted{3}, OrderCancelled{3, 10}}));
}

TEST_P(EngineTest, FOKNeverRests) {
    run(gtc(1, Sell, 4, 100));
    run(order(2, Buy, OrderType::LimitFOK, 10, 100));

    EXPECT_EQ(run(CancelOrder{.id = 2}), (Events{CancelRejected{2, RejectReason::UnknownOrder}}));
}

// --- MarketFOK ---

TEST_P(EngineTest, MarketFOKFillsAtAnyPrice) {
    run(gtc(1, Sell, 5, 100));
    run(gtc(2, Sell, 5, 500));

    EXPECT_EQ(run(order(3, Buy, OrderType::MarketFOK, 10, std::nullopt)),
              (Events{OrderAccepted{3}, Trade{3, 1, Buy, 100, 5}, Trade{3, 2, Buy, 500, 5}}));
}

TEST_P(EngineTest, MarketFOKKilledOnThinBook) {
    run(gtc(1, Sell, 4, 100));

    EXPECT_EQ(run(order(2, Buy, OrderType::MarketFOK, 10, std::nullopt)),
              (Events{OrderAccepted{2}, OrderCancelled{2, 10}}));
    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 4}}));
}

// --- Cancels ---

TEST_P(EngineTest, CancelResting) {
    run(gtc(1, Buy, 10, 100));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 10}}));
}

TEST_P(EngineTest, CancelledOrderLeavesBook) {
    run(gtc(1, Buy, 10, 100));
    run(CancelOrder{.id = 1});

    EXPECT_EQ(run(gtc(2, Sell, 10, 100)), (Events{OrderAccepted{2}, OrderRested{2, 100, 10}}));
}

TEST_P(EngineTest, CancelPartiallyFilled) {
    run(gtc(1, Sell, 10, 100));
    run(gtc(2, Buy, 4, 100));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 6}}));
}

TEST_P(EngineTest, CancelUnknownId) {
    EXPECT_EQ(run(CancelOrder{.id = 42}), (Events{CancelRejected{42, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, CancelTwice) {
    run(gtc(1, Buy, 10, 100));
    run(CancelOrder{.id = 1});

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{CancelRejected{1, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, CancelRejectedOrder) {
    run(gtc(1, Buy, 0, 100));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{CancelRejected{1, RejectReason::UnknownOrder}}));
}

TEST_P(EngineTest, CancelOnOtherSymbol) {
    run(gtc(1, Buy, 10, 100, "MSFT"));

    EXPECT_EQ(run(CancelOrder{.id = 1}), (Events{OrderCancelled{1, 10}}));
}
