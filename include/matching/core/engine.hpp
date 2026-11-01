#pragma once

#include <concepts>
#include <cstddef>

#include "matching/core/command.hpp"
#include "matching/core/event_sink.hpp"

namespace matching {

// The interface every engine provides, so tests and tools can drive any of them.
// Built from the number of symbols (valid IDs are 0 to count - 1), then processes one command at
// a time, sending exactly one batch to the sink for each. process() takes any EventSink; the
// concept checks it with BatchCollector.
template <typename E>
concept Engine = std::constructible_from<E, std::size_t> &&
                 requires(E& engine, const EngineCommand& command, BatchCollector& sink) {
                     engine.process(command, sink);
                 };

} // namespace matching
