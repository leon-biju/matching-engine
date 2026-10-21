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
        .quantity = 10,
        .price = 100,
    });
    const auto second = engine.process(CancelOrder{.id = 1});

    EXPECT_EQ(first.sequence, 1u);
    EXPECT_EQ(second.sequence, 2u);
}
