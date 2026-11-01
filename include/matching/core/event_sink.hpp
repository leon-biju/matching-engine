#pragma once

#include <vector>

#include "matching/core/event.hpp"
#include "matching/core/types.hpp"

namespace matching {

// Where an engine sends the events of each command. For every command the engine calls
// begin_batch once with the command's sequence, add once per event in order, then end_batch.
template <typename S>
concept EventSink = requires(S& sink, Sequence sequence, const Event& event) {
    sink.begin_batch(sequence);
    sink.add(event);
    sink.end_batch();
};

// Collects every batch into a vector. For tests and tools that want whole batches.
class BatchCollector {
  public:
    void begin_batch(Sequence sequence) {
        batches_.push_back({.sequence = sequence, .events = {}});
    }
    void add(const Event& event) { batches_.back().events.push_back(event); }
    void end_batch() {}

    std::vector<EventBatch>& batches() { return batches_; }

  private:
    std::vector<EventBatch> batches_;
};

static_assert(EventSink<BatchCollector>);

} // namespace matching
