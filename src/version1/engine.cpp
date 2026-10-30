#include "matching/version1/engine.hpp"

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace matching::version1 {

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

} // namespace

ReferenceEngine::ReferenceEngine(std::vector<std::string> symbols) {
    books_.resize(symbols.size());
    for (SymbolId id = 0; id < symbols.size(); ++id) {
        if (!symbol_ids_.emplace(std::move(symbols[id]), id).second) {
            throw std::invalid_argument("duplicate symbol");
        }
    }
}

EventBatch ReferenceEngine::process(const EngineCommand& command) {
    // The sequence starts at 1, so it only reaches 0 by wrapping.
    if (next_sequence_ == 0) {
        throw std::overflow_error("engine sequence exhausted");
    }
    EventBatch batch{.sequence = next_sequence_++, .events = {}};

    std::visit(
        [&](const auto& cmd) {
            if constexpr (std::is_same_v<std::decay_t<decltype(cmd)>, NewOrder>) {
                handle_new(batch.sequence, cmd, batch.events);
            } else {
                handle_cancel(cmd, batch.events);
            }
        },
        command);
    return batch;
}

void ReferenceEngine::handle_new(Sequence sequence, const NewOrder& order,
                                 std::vector<Event>& events) {
    const auto reject = [&](RejectReason reason) {
        events.push_back(OrderRejected{.id = order.id, .reason = reason});
    };

    // Validation, in the order given in semantics.md.
    if (accepted_ids_.contains(order.id)) {
        return reject(RejectReason::DuplicateOrderId);
    }
    const auto symbol = symbol_ids_.find(order.symbol);
    if (symbol == symbol_ids_.end()) {
        return reject(RejectReason::UnknownSymbol);
    }
    if (order.qty == 0) {
        return reject(RejectReason::InvalidQuantity);
    }
    const bool market = is_market(order.type);
    if (market ? order.price.has_value() : order.price.value_or(0) == 0) {
        return reject(RejectReason::InvalidPrice);
    }
    OrderBook& book = books_[symbol->second];
    if (market && book.side_empty(opposite(order.side))) {
        return reject(RejectReason::NoLiquidity);
    }

    accepted_ids_.insert(order.id);
    events.push_back(OrderAccepted{.id = order.id});

    if (is_fok(order.type) && !book.can_fill(order.side, order.price, order.qty)) {
        events.push_back(OrderCancelled{.id = order.id, .remaining = order.qty});
        return;
    }

    const Quantity remaining =
        book.match(order.id, order.side, order.price, order.qty, events,
                   [this](const Order& filled) { resting_.erase(filled.id); });
    if (remaining == 0) {
        return;
    }

    if (order.type == OrderType::LimitGTC) {
        const auto handle = book.add(Order{
            .id = order.id,
            .symbol = symbol->second,
            .side = order.side,
            .price = *order.price,
            .remaining_qty = remaining,
            .sequence = sequence,
        });
        resting_.emplace(order.id, handle);
        events.push_back(
            OrderRested{.id = order.id, .price = *order.price, .remaining = remaining});
    } else {
        events.push_back(OrderCancelled{.id = order.id, .remaining = remaining});
    }
}

void ReferenceEngine::handle_cancel(const CancelOrder& cancel, std::vector<Event>& events) {
    const auto it = resting_.find(cancel.id);
    if (it == resting_.end()) {
        events.push_back(CancelRejected{.id = cancel.id, .reason = RejectReason::UnknownOrder});
        return;
    }

    const OrderBook::Handle handle = it->second;
    events.push_back(OrderCancelled{.id = cancel.id, .remaining = handle->remaining_qty});
    books_[handle->symbol].remove(handle);
    resting_.erase(it);
}

} // namespace matching::version1
