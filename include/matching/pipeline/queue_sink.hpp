#pragma once

#include <variant>

#include "matching/core/event.hpp"
#include "matching/core/event_sink.hpp"
#include "matching/core/types.hpp"
#include "matching/pipeline/spsc_queue.hpp"

namespace matching::pipeline {

// Marks the start of a command's batch in an event queue.
struct BatchBegin {
    Sequence sequence;

    bool operator==(const BatchBegin&) const = default;
};

// Marks the end of the batch that the last BatchBegin started.
struct BatchEnd {
    bool operator==(const BatchEnd&) const = default;
};

// One item in the queue from the engine to the event sink. Each batch crosses the queue as a
// BatchBegin, then its events in order, then a BatchEnd, so one item matches one EventSink call.
// The engine sends batches one at a time in sequence order, so the consumer can rebuild exactly
// one batch per command.
using SinkItem = std::variant<BatchBegin, Event, BatchEnd>;

// An EventSink that writes each batch into an SPSC queue, for the sink thread to read. It waits
// while the queue is full. The engine thread is the queue's producer.
class QueueSink {
  public:
    explicit QueueSink(SpscQueue<SinkItem>& queue) : queue_(&queue) {}

    void begin_batch(Sequence sequence) { queue_->push(BatchBegin{.sequence = sequence}); }
    void add(const Event& event) { queue_->push(SinkItem(std::in_place_type<Event>, event)); }
    void end_batch() { queue_->push(BatchEnd{}); }

  private:
    SpscQueue<SinkItem>* queue_;
};

static_assert(EventSink<QueueSink>);

// The consumer side of a QueueSink: makes the EventSink call on `sink` that `item` stands for.
// Feeding it every item popped from the queue, in order, replays the engine's calls exactly.
// Returns true if the item ended a batch.
template <EventSink Sink> bool replay(const SinkItem& item, Sink& sink) {
    if (const auto* begin = std::get_if<BatchBegin>(&item)) {
        sink.begin_batch(begin->sequence);
        return false;
    }
    if (const auto* event = std::get_if<Event>(&item)) {
        sink.add(*event);
        return false;
    }
    sink.end_batch();
    return true;
}

} // namespace matching::pipeline
