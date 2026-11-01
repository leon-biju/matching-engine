#pragma once

#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "matching/core/command.hpp"
#include "matching/core/engine.hpp"
#include "matching/core/event.hpp"
#include "matching/core/event_sink.hpp"
#include "matching/version1/order_book.hpp"

namespace matching::version1 {

class ReferenceEngine {
  public:
    // Symbol IDs 0 to symbol_count - 1 are valid.
    explicit ReferenceEngine(std::size_t symbol_count);

    // Processes one command and sends exactly one batch of the events it caused to `sink`.
    template <EventSink Sink> void process(const EngineCommand& command, Sink& sink) {
        const Sequence sequence = handle(command);
        sink.begin_batch(sequence);
        for (const Event& event : events_) {
            sink.add(event);
        }
        sink.end_batch();
    }

  private:
    // Processes one command into events_ and returns its sequence.
    Sequence handle(const EngineCommand& command);
    void handle_new(Sequence sequence, const NewOrder& order, std::vector<Event>& events);
    void handle_cancel(const CancelOrder& cancel, std::vector<Event>& events);

    std::vector<OrderBook> books_;             // Indexed by SymbolId
    std::unordered_set<OrderId> accepted_ids_; // Every ID ever accepted, never cleared
    std::unordered_map<OrderId, OrderBook::Handle> resting_;
    std::vector<Event> events_; // The current command's events, reused between commands
    Sequence next_sequence_ = 1;
};

static_assert(Engine<ReferenceEngine>);

} // namespace matching::version1
