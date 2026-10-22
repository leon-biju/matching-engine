#include <gtest/gtest.h>

#include "matching/version1/engine.hpp"

using namespace matching;

TEST(ReferenceEngine, SequenceStartsAtOneAndIncrements) {
    version1::ReferenceEngine engine({"AAPL"});

    const auto first = engine.process(NewOrder{
        .id = 1,
        .symbol = "AAPL",
        .side = OrderSide::Buy,
        .type = OrderType::LimitGTC,
        .qty = 10,
        .price = 100,
    });
    const auto second = engine.process(CancelOrder{.id = 1});

    EXPECT_EQ(first.sequence, 1u);
    EXPECT_EQ(second.sequence, 2u);
}

// Planned tests, to be written formally. Each one follows documentation/semantics.md.
//
// Shorthand:
//   #1 Buy GTC 10@100   -> order id 1, buy LimitGTC, quantity 10, price 100, symbol AAPL
//   Trade(2->1, Buy, 10@100) -> aggressor 2 hit resting 1, aggressor side Buy, 10 at 100
//   Expect [ ... ]      -> the exact events in that command's batch, in order
//
// --- Sequencing ---
// RejectedCommandUsesSequence: #1 for unknown symbol (rejected), then a valid #2.
//   Expect sequences 1 and 2.
//
// --- Validation ---
// UnknownSymbol: #1 for "MSFT" on an AAPL-only engine. Expect [Rejected(1, UnknownSymbol)].
// SymbolIsCaseSensitive: #1 for "aapl". Expect [Rejected(1, UnknownSymbol)].
// ZeroQuantity: #1 Buy GTC 0@100. Expect [Rejected(1, InvalidQuantity)].
// LimitWithoutPrice: #1 Buy GTC 10, no price. Expect [Rejected(1, InvalidPrice)].
// LimitWithZeroPrice: #1 Buy GTC 10@0. Expect [Rejected(1, InvalidPrice)].
// MarketWithPrice: #1 Buy MarketIOC 10@100. Expect [Rejected(1, InvalidPrice)].
// MarketIOCOnEmptyBook: #1 Buy MarketIOC 10. Expect [Rejected(1, NoLiquidity)].
// MarketFOKOnEmptyBook: #1 Buy MarketFOK 10. Expect [Rejected(1, NoLiquidity)].
// NoLiquidityLooksAtOppositeSideOnly: #1 Buy GTC 10@100 rests, then #2 Buy MarketIOC 10.
//   Expect [Rejected(2, NoLiquidity)].
// DuplicateIdOfRestingOrder: #1 rests, then another #1. Expect [Rejected(1, DuplicateOrderId)].
// DuplicateIdOfFilledOrder: #1 fully filled, then another #1. Expect DuplicateOrderId.
// DuplicateIdOfCancelledOrder: #1 rests and is cancelled, then another #1. Expect DuplicateOrderId.
// DuplicateIdAcrossSymbols: #1 rests on AAPL, then #1 on MSFT. Expect DuplicateOrderId.
// RejectedIdCanBeReused: #1 rejected (zero quantity), then a valid #1. Expect [Accepted(1), Rested(1, ...)].
// RejectedOrderLeavesBookUntouched: #1 Sell GTC 10@100 rests, then #1 Buy GTC 10@100 (duplicate).
//   Expect no trade, and #1 is still resting (a later crossing order fills against it).
//
// Validation order (the first failing check wins):
// DuplicateBeatsUnknownSymbol: #1 rests, then #1 for "MSFT". Expect DuplicateOrderId.
// UnknownSymbolBeatsQuantity: #1 for "MSFT" with quantity 0. Expect UnknownSymbol.
// QuantityBeatsPrice: #1 Buy GTC 0@0. Expect InvalidQuantity.
// PriceBeatsNoLiquidity: #1 Buy MarketIOC 10@100 on an empty book. Expect InvalidPrice.
//
// --- Resting ---
// GTCRestsOnEmptyBook: #1 Buy GTC 10@100. Expect [Accepted(1), Rested(1, 100, 10)].
// NonCrossingGTCRests: #1 Sell GTC 10@101 rests, #2 Buy GTC 10@100.
//   Expect [Accepted(2), Rested(2, 100, 10)].
// SymbolsHaveSeparateBooks: #1 Sell GTC 10@100 on AAPL, #2 Buy GTC 10@100 on MSFT.
//   Expect #2 rests with no trade.
//
// --- Matching ---
// ExactFill: #1 Sell GTC 10@100, #2 Buy GTC 10@100.
//   Expect [Accepted(2), Trade(2->1, Buy, 10@100)], with no final event.
// FilledRestingOrderLeavesBook: after ExactFill, Cancel #1. Expect [CancelRejected(1, UnknownOrder)].
// TradesAtRestingPriceForBuy: #1 Sell GTC 10@100, #2 Buy GTC 10@105. Expect Trade(2->1, Buy, 10@100).
// TradesAtRestingPriceForSell: #1 Buy GTC 10@100, #2 Sell GTC 10@95. Expect Trade(2->1, Sell, 10@100).
// IncomingRemainderRests: #1 Sell GTC 4@100, #2 Buy GTC 10@100.
//   Expect [Accepted(2), Trade(2->1, Buy, 4@100), Rested(2, 100, 6)].
// RestingRemainderStays: #1 Sell GTC 10@100, #2 Buy GTC 4@100, #3 Buy GTC 6@100.
//   Expect #3 gets Trade(3->1, Buy, 6@100), and #1 is then gone.
// SweepsBestPriceFirst: #1 Sell GTC 5@101, #2 Sell GTC 5@100, #3 Buy GTC 10@101.
//   Expect [Accepted(3), Trade(3->2, Buy, 5@100), Trade(3->1, Buy, 5@101)].
// SellSweepsHighestBidFirst: #1 Buy GTC 5@100, #2 Buy GTC 5@101, #3 Sell GTC 10@100.
//   Expect [Accepted(3), Trade(3->2, Sell, 5@101), Trade(3->1, Sell, 5@100)].
// StopsAtLimitPrice: #1 Sell GTC 5@100, #2 Sell GTC 5@102, #3 Buy GTC 10@101.
//   Expect [Accepted(3), Trade(3->1, Buy, 5@100), Rested(3, 101, 5)], and #2 untouched.
// TimePriorityWithinLevel: #1 Sell GTC 5@100, #2 Sell GTC 5@100, #3 Buy GTC 5@100.
//   Expect Trade(3->1, ...), not #2.
// PartialFillKeepsQueuePosition: #1 Sell GTC 10@100, #2 Sell GTC 10@100,
//   #3 Buy GTC 4@100, #4 Buy GTC 4@100. Expect both #3 and #4 trade against #1.
// OneTradePerRestingOrder: #1 Sell GTC 3@100, #2 Sell GTC 3@100, #3 Buy GTC 6@100.
//   Expect exactly two trades: (3->1, 3) then (3->2, 3).
//
// --- LimitIOC ---
// IOCFullFill: #1 Sell GTC 10@100, #2 Buy IOC 10@100.
//   Expect [Accepted(2), Trade(2->1, Buy, 10@100)], with no final event.
// IOCRemainderCancelled: #1 Sell GTC 4@100, #2 Buy IOC 10@100.
//   Expect [Accepted(2), Trade(2->1, Buy, 4@100), Cancelled(2, 6)].
// IOCNoCross: #1 Sell GTC 10@101, #2 Buy IOC 10@100. Expect [Accepted(2), Cancelled(2, 10)].
// IOCNeverRests: after IOCRemainderCancelled, Cancel #2. Expect [CancelRejected(2, UnknownOrder)].
//
// --- MarketIOC ---
// MarketIOCIgnoresPrice: #1 Sell GTC 5@100, #2 Sell GTC 5@500, #3 Buy MarketIOC 10.
//   Expect [Accepted(3), Trade(3->1, Buy, 5@100), Trade(3->2, Buy, 5@500)].
// MarketIOCRemainderCancelled: #1 Sell GTC 4@100, #2 Buy MarketIOC 10.
//   Expect [Accepted(2), Trade(2->1, Buy, 4@100), Cancelled(2, 6)].
//
// --- LimitFOK ---
// FOKFills: #1 Sell GTC 10@100, #2 Buy FOK 10@100.
//   Expect [Accepted(2), Trade(2->1, Buy, 10@100)].
// FOKFillsAcrossLevels: #1 Sell GTC 5@100, #2 Sell GTC 5@101, #3 Buy FOK 10@101.
//   Expect [Accepted(3), Trade(3->1, ...5@100), Trade(3->2, ...5@101)].
// FOKKilledWhenTooSmall: #1 Sell GTC 4@100, #2 Buy FOK 10@100.
//   Expect [Accepted(2), Cancelled(2, 10)], and #1 still has all 4 resting.
// FOKCountsOnlyCrossingQuantity: #1 Sell GTC 5@100, #2 Sell GTC 5@105, #3 Buy FOK 10@100.
//   Expect [Accepted(3), Cancelled(3, 10)], with no trades.
// FOKNeverRests: after FOKKilledWhenTooSmall, Cancel #2. Expect [CancelRejected(2, UnknownOrder)].
//
// --- MarketFOK ---
// MarketFOKFillsAtAnyPrice: #1 Sell GTC 5@100, #2 Sell GTC 5@500, #3 Buy MarketFOK 10.
//   Expect [Accepted(3), Trade(3->1, ...5@100), Trade(3->2, ...5@500)].
// MarketFOKKilledOnThinBook: #1 Sell GTC 4@100, #2 Buy MarketFOK 10.
//   Expect [Accepted(2), Cancelled(2, 10)], with no trades and #1 untouched.
//
// --- Cancels ---
// CancelResting: #1 Buy GTC 10@100 rests, Cancel #1. Expect [Cancelled(1, 10)].
// CancelledOrderLeavesBook: after CancelResting, #2 Sell GTC 10@100. Expect #2 rests, no trade.
// CancelPartiallyFilled: #1 Sell GTC 10@100, #2 Buy GTC 4@100, Cancel #1. Expect [Cancelled(1, 6)].
// CancelUnknownId: Cancel #42 on a fresh engine. Expect [CancelRejected(42, UnknownOrder)].
// CancelTwice: #1 rests, Cancel #1, Cancel #1 again. Expect the second is [CancelRejected(1, UnknownOrder)].
// CancelRejectedOrder: #1 rejected (zero quantity), Cancel #1. Expect [CancelRejected(1, UnknownOrder)].
// CancelOnOtherSymbol: #1 rests on MSFT, Cancel #1. Expect [Cancelled(1, 10)].
