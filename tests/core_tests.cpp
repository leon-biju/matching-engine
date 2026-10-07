#include <matchengine/core/command.hpp>
#include <matchengine/core/event.hpp>

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <variant>

namespace matchengine {
namespace {

TEST(OrderTest, DefaultsToEmptyBuyLimitGTC) {
    const Order order{};

    EXPECT_EQ(order.order_id, 0u);
    EXPECT_TRUE(order.symbol.empty());
    EXPECT_EQ(order.side, OrderSide::Buy);
    EXPECT_EQ(order.type, OrderType::LimitGTC);
    EXPECT_EQ(order.qty, 0u);
    EXPECT_FALSE(order.price.has_value());
}

TEST(OrderTest, PriceDistinguishesAbsentFromZero) {
    Order order{1, "ABC", OrderSide::Sell, OrderType::MarketIOC, 10, std::nullopt};
    EXPECT_FALSE(order.price.has_value());

    order.price = 0;
    ASSERT_TRUE(order.price.has_value());
    EXPECT_EQ(*order.price, 0u);
}

TEST(OrderTest, EqualityComparesEveryField) {
    const Order order{1, "ABC", OrderSide::Sell, OrderType::LimitIOC, 10, 100};
    auto other = order;
    EXPECT_EQ(order, other);

    other.order_id = 2;
    EXPECT_NE(order, other);
    other = order;
    other.symbol = "XYZ";
    EXPECT_NE(order, other);
    other = order;
    other.side = OrderSide::Buy;
    EXPECT_NE(order, other);
    other = order;
    other.type = OrderType::LimitFOK;
    EXPECT_NE(order, other);
    other = order;
    other.qty = 11;
    EXPECT_NE(order, other);
    other = order;
    other.price = 101;
    EXPECT_NE(order, other);
    other.price.reset();
    EXPECT_NE(order, other);
}

TEST(CommandTest, DefaultsToNewOrder) {
    const Command command{};

    ASSERT_TRUE(std::holds_alternative<NewOrder>(command));
    EXPECT_EQ(std::get<NewOrder>(command), Order{});
}

TEST(CommandTest, HoldsNewOrderAndCancelOrder) {
    const NewOrder order{42, "ABC", OrderSide::Buy, OrderType::LimitGTC, 10, 100};
    Command command = order;
    ASSERT_TRUE(std::holds_alternative<NewOrder>(command));
    EXPECT_EQ(std::get<NewOrder>(command), order);

    command = CancelOrder{42};
    ASSERT_TRUE(std::holds_alternative<CancelOrder>(command));
    EXPECT_EQ(std::get<CancelOrder>(command).order_id, 42u);
}

TEST(CommandTest, EqualityComparesAlternativeAndContents) {
    EXPECT_EQ(CancelOrder{}.order_id, 0u);
    EXPECT_EQ(CancelOrder{42}, CancelOrder{42});
    EXPECT_NE(CancelOrder{42}, CancelOrder{43});
    EXPECT_EQ(Command{CancelOrder{42}}, Command{CancelOrder{42}});
    EXPECT_NE(Command{CancelOrder{42}}, Command{CancelOrder{43}});
    EXPECT_NE(Command{NewOrder{}}, Command{CancelOrder{}});
}

TEST(EventTest, DefaultsToEmptyOrderAccepted) {
    const Event event{};

    EXPECT_EQ(event.event_index, 0u);
    EXPECT_FALSE(event.symbol.has_value());
    ASSERT_TRUE(std::holds_alternative<OrderAccepted>(event.payload));
    const auto& accepted = std::get<OrderAccepted>(event.payload);
    EXPECT_EQ(accepted.order_id, 0u);
    EXPECT_EQ(accepted.side, OrderSide::Buy);
    EXPECT_EQ(accepted.type, OrderType::LimitGTC);
    EXPECT_EQ(accepted.original_qty, 0u);
    EXPECT_FALSE(accepted.price.has_value());
}

TEST(EventTest, SupportsEveryImplementedPayload) {
    const std::array<EventPayload, 6> payloads{
        OrderAccepted{1, OrderSide::Buy, OrderType::LimitGTC, 10, 100},
        OrderRejected{2, OrderRejectReason::InvalidQuantity},
        Trade{1, 3, 100, 5, OrderSide::Sell},
        OrderRested{1, 100, 5},
        OrderCancelled{1, 5, CancelReason::UserRequested},
        CancelRejected{4, CancelRejectReason::UnknownOrder},
    };

    for (std::size_t i = 0; i < payloads.size(); ++i) {
        SCOPED_TRACE(i);
        const Event event{7, "ABC", payloads[i]};
        EXPECT_EQ(event.payload.index(), i);
        EXPECT_EQ(event.payload, payloads[i]);
        EXPECT_EQ(event, (Event{7, "ABC", payloads[i]}));
    }
}

TEST(EventTest, EqualityComparesIndexSymbolAndPayload) {
    const Event event{7, "ABC", OrderRested{1, 100, 5}};
    auto other = event;
    EXPECT_EQ(event, other);

    other.event_index = 8;
    EXPECT_NE(event, other);
    other = event;
    other.symbol = "XYZ";
    EXPECT_NE(event, other);
    other.symbol.reset();
    EXPECT_NE(event, other);
    other = event;
    other.payload = OrderRested{1, 100, 6};
    EXPECT_NE(event, other);
    other.payload = OrderCancelled{1, 5, CancelReason::UserRequested};
    EXPECT_NE(event, other);
}

TEST(EventPayloadTest, AcceptedOrderEqualityIncludesOptionalPrice) {
    const OrderAccepted accepted{1, OrderSide::Buy, OrderType::MarketIOC, 10, std::nullopt};
    auto other = accepted;
    EXPECT_EQ(accepted, other);
    other.price = 0;
    EXPECT_NE(accepted, other);
}

TEST(EventPayloadTest, TradeEqualityComparesEveryField) {
    const Trade trade{1, 2, 100, 5, OrderSide::Sell};
    auto other = trade;
    EXPECT_EQ(trade, other);

    other.maker_order_id = 3;
    EXPECT_NE(trade, other);
    other = trade;
    other.taker_order_id = 3;
    EXPECT_NE(trade, other);
    other = trade;
    other.price = 101;
    EXPECT_NE(trade, other);
    other = trade;
    other.qty = 6;
    EXPECT_NE(trade, other);
    other = trade;
    other.aggressor_side = OrderSide::Buy;
    EXPECT_NE(trade, other);
}

} // namespace (anon)
} // namespace matchengine
