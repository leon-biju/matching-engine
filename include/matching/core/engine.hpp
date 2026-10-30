#pragma once

#include <concepts>
#include <string>
#include <vector>

#include "matching/core/command.hpp"
#include "matching/core/event.hpp"

namespace matching {

// The interface every engine provides, so tests and tools can drive any of them.
// Built from the fixed symbol set, then processes one command into exactly one batch.
template <typename E>
concept Engine = std::constructible_from<E, std::vector<std::string>> &&
                 requires(E& engine, const EngineCommand& command) {
                     { engine.process(command) } -> std::same_as<EventBatch>;
                 };

} // namespace matching
