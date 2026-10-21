#include "matching/version1/engine.hpp"

#include <utility>

namespace matching::version1 {

ReferenceEngine::ReferenceEngine(std::vector<std::string> symbols) : symbols_(std::move(symbols)) {}

EventBatch ReferenceEngine::process(const EngineCommand& /*command*/) {
    // TODO: validation and matching, once the rules in semantics.md are settled.
    return EventBatch{.sequence = next_sequence_++, .events = {}};
}

} // namespace matching::version1
