#pragma once

#include <string>
#include <vector>

#include "matching/core/command.hpp"
#include "matching/core/event.hpp"

namespace matching::version1 {

class ReferenceEngine {
public:
    explicit ReferenceEngine(std::vector<std::string> symbols);

    // Processes one command and returns exactly one batch of the events it caused.
    EventBatch process(const EngineCommand& command);

private:
    std::vector<std::string> symbols_;
    Sequence next_sequence_ = 1;
};

} // namespace matching::version1
