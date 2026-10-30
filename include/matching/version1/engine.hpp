#pragma once

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "matching/core/command.hpp"
#include "matching/core/engine.hpp"
#include "matching/core/event.hpp"
#include "matching/version1/order_book.hpp"

namespace matching::version1 {

class ReferenceEngine {
public:
    // Symbol IDs 0 to symbol_count - 1 are valid.
    explicit ReferenceEngine(std::size_t symbol_count);

    // Processes one command and returns exactly one batch of the events it caused.
    EventBatch process(const EngineCommand& command);

private:
    void handle_new(Sequence sequence, const NewOrder& order, std::vector<Event>& events);
    void handle_cancel(const CancelOrder& cancel, std::vector<Event>& events);

    std::vector<OrderBook> books_;             // Indexed by SymbolId
    std::unordered_set<OrderId> accepted_ids_; // Every ID ever accepted, never cleared
    std::unordered_map<OrderId, OrderBook::Handle> resting_;
    Sequence next_sequence_ = 1;
};

static_assert(Engine<ReferenceEngine>);

} // namespace matching::version1
