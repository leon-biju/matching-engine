#pragma once

#include <gtest/gtest.h>

#include <ostream>

#include "matching/core/command.hpp"
#include "matching/core/event.hpp"

// Readable gtest output for commands and events, found by ADL.

namespace matching {

inline const char* to_string(OrderSide side) {
    return side == OrderSide::Buy ? "Buy" : "Sell";
}

inline const char* to_string(OrderType type) {
    switch (type) {
    case OrderType::LimitGTC:
        return "LimitGTC";
    case OrderType::LimitIOC:
        return "LimitIOC";
    case OrderType::LimitFOK:
        return "LimitFOK";
    case OrderType::MarketIOC:
        return "MarketIOC";
    case OrderType::MarketFOK:
        return "MarketFOK";
    }
    return "?";
}

inline const char* to_string(RejectReason reason) {
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

inline void PrintTo(const NewOrder& c, std::ostream* os) {
    *os << "NewOrder(" << c.id << ", symbol " << c.symbol << ", " << to_string(c.side) << ", "
        << to_string(c.type) << ", " << c.qty << "@";
    if (c.price) {
        *os << *c.price;
    } else {
        *os << "market";
    }
    *os << ")";
}

inline void PrintTo(const CancelOrder& c, std::ostream* os) {
    *os << "CancelOrder(" << c.id << ")";
}

inline void PrintTo(const OrderAccepted& e, std::ostream* os) {
    *os << "Accepted(" << e.id << ")";
}

inline void PrintTo(const OrderRejected& e, std::ostream* os) {
    *os << "Rejected(" << e.id << ", " << to_string(e.reason) << ")";
}

inline void PrintTo(const Trade& e, std::ostream* os) {
    *os << "Trade(" << e.aggressor_id << "->" << e.resting_id << ", " << to_string(e.aggressor_side)
        << ", " << e.quantity << "@" << e.price << ")";
}

inline void PrintTo(const OrderRested& e, std::ostream* os) {
    *os << "Rested(" << e.id << ", " << e.price << ", " << e.remaining << ")";
}

inline void PrintTo(const OrderCancelled& e, std::ostream* os) {
    *os << "Cancelled(" << e.id << ", " << e.remaining << ")";
}

inline void PrintTo(const CancelRejected& e, std::ostream* os) {
    *os << "CancelRejected(" << e.id << ", " << to_string(e.reason) << ")";
}

inline void PrintTo(const EventBatch& b, std::ostream* os) {
    *os << "EventBatch(" << b.sequence << ", " << ::testing::PrintToString(b.events) << ")";
}

} // namespace matching
