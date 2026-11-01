#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

#include "matching/core/command.hpp"
#include "matching/core/event.hpp"
#include "matching/core/event_sink.hpp"
#include "matching/pipeline/queue_sink.hpp"
#include "matching/pipeline/spsc_queue.hpp"
#include "matching/version1/engine.hpp"
#include "matching/workload/command_generator.hpp"
#include "printers.hpp"

using namespace matching;
using namespace matching::pipeline;

TEST(SpscQueueTest, RejectsCapacityThatIsNotAPowerOfTwo) {
    EXPECT_THROW(SpscQueue<int>(0), std::invalid_argument);
    EXPECT_THROW(SpscQueue<int>(3), std::invalid_argument);
    EXPECT_THROW(SpscQueue<int>(12), std::invalid_argument);
    EXPECT_EQ(SpscQueue<int>(1).capacity(), 1);
    EXPECT_EQ(SpscQueue<int>(16).capacity(), 16);
}

TEST(SpscQueueTest, PopsInFifoOrder) {
    SpscQueue<int> queue(8);
    for (int i = 1; i <= 5; ++i) {
        ASSERT_TRUE(queue.try_push(i));
    }
    std::vector<int> popped;
    while (const std::optional<int> value = queue.try_pop()) {
        popped.push_back(*value);
    }
    EXPECT_EQ(popped, (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST(SpscQueueTest, EmptyQueuePopsNothing) {
    SpscQueue<int> queue(4);
    EXPECT_EQ(queue.try_pop(), std::nullopt);

    ASSERT_TRUE(queue.try_push(7));
    EXPECT_EQ(queue.try_pop(), 7);
    EXPECT_EQ(queue.try_pop(), std::nullopt);
}

TEST(SpscQueueTest, FullQueueRejectsPushAndKeepsItsContents) {
    SpscQueue<int> queue(4);
    for (int i = 1; i <= 4; ++i) {
        ASSERT_TRUE(queue.try_push(i));
    }
    EXPECT_FALSE(queue.try_push(5));

    std::vector<int> popped;
    while (const std::optional<int> value = queue.try_pop()) {
        popped.push_back(*value);
    }
    EXPECT_EQ(popped, (std::vector<int>{1, 2, 3, 4}));
}

TEST(SpscQueueTest, PopFreesASlotForPush) {
    SpscQueue<int> queue(2);
    ASSERT_TRUE(queue.try_push(1));
    ASSERT_TRUE(queue.try_push(2));
    EXPECT_FALSE(queue.try_push(3));

    EXPECT_EQ(queue.try_pop(), 1);
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_FALSE(queue.try_push(4));
    EXPECT_EQ(queue.try_pop(), 2);
    EXPECT_EQ(queue.try_pop(), 3);
    EXPECT_EQ(queue.try_pop(), std::nullopt);
}

// Pushes and pops three at a time through four slots, so the indices wrap round the slots many
// times at every offset.
TEST(SpscQueueTest, KeepsOrderAcrossWraparound) {
    SpscQueue<int> queue(4);
    std::vector<int> popped;
    int next = 0;
    for (int round = 0; round < 100; ++round) {
        for (int i = 0; i < 3; ++i) {
            ASSERT_TRUE(queue.try_push(next++));
        }
        for (int i = 0; i < 3; ++i) {
            const std::optional<int> value = queue.try_pop();
            ASSERT_TRUE(value.has_value());
            popped.push_back(*value);
        }
    }
    std::vector<int> expected;
    for (int i = 0; i < next; ++i) {
        expected.push_back(i);
    }
    EXPECT_EQ(popped, expected);
    EXPECT_EQ(queue.try_pop(), std::nullopt);
}

// A producer and a consumer thread pass many items through a small queue, so both often find it
// full or empty. The consumer must receive every item once, in order.
TEST(SpscQueueTest, PassesEveryItemInOrderBetweenTwoThreads) {
    constexpr std::uint64_t count = 200'000;
    SpscQueue<std::uint64_t> queue(64);

    std::thread producer([&queue] {
        for (std::uint64_t i = 0; i < count; ++i) {
            queue.push(i);
        }
    });

    std::uint64_t received = 0;
    std::uint64_t out_of_order = 0;
    for (std::uint64_t i = 0; i < count; ++i) {
        if (queue.pop() != i) {
            ++out_of_order;
        }
        ++received;
    }
    producer.join();

    EXPECT_EQ(received, count);
    EXPECT_EQ(out_of_order, 0);
    EXPECT_EQ(queue.try_pop(), std::nullopt);
}

namespace {

std::vector<EngineCommand> generate(std::uint64_t seed, workload::GeneratorConfig config,
                                    std::size_t count) {
    workload::CommandGenerator generator(seed, config);
    std::vector<EngineCommand> commands;
    for (std::size_t i = 0; i < count; ++i) {
        commands.push_back(generator.next());
    }
    return commands;
}

std::vector<EventBatch> collect_directly(const std::vector<EngineCommand>& commands,
                                         std::size_t symbol_count) {
    version1::ReferenceEngine engine(symbol_count);
    BatchCollector collector;
    for (const EngineCommand& command : commands) {
        engine.process(command, collector);
    }
    return collector.batches();
}

// Runs the engine on its own thread with a QueueSink, and rebuilds the batches on this thread by
// replaying the queue into a BatchCollector. The small queue makes the engine wait while it is
// full.
std::vector<EventBatch> collect_through_queue(const std::vector<EngineCommand>& commands,
                                              std::size_t symbol_count) {
    SpscQueue<SinkItem> queue(16);
    std::thread engine_thread([&] {
        version1::ReferenceEngine engine(symbol_count);
        QueueSink sink(queue);
        for (const EngineCommand& command : commands) {
            engine.process(command, sink);
        }
    });

    BatchCollector collector;
    std::size_t batches = 0;
    while (batches < commands.size()) {
        if (replay(queue.pop(), collector)) {
            ++batches;
        }
    }
    engine_thread.join();
    EXPECT_EQ(queue.try_pop(), std::nullopt);
    return collector.batches();
}

} // namespace

TEST(QueueSinkTest, RebuildsTheSameBatchesAsBatchCollector) {
    const workload::GeneratorConfig config{};
    const std::vector<EngineCommand> commands = generate(5, config, 2000);
    EXPECT_EQ(collect_through_queue(commands, config.symbol_count),
              collect_directly(commands, config.symbol_count));
}

// The aggressive profile's orders mostly trade, so its batches often hold several events.
TEST(QueueSinkTest, RebuildsMultiEventBatches) {
    const workload::GeneratorConfig config = workload::profiles::aggressive;
    const std::vector<EngineCommand> commands = generate(6, config, 2000);
    EXPECT_EQ(collect_through_queue(commands, config.symbol_count),
              collect_directly(commands, config.symbol_count));
}
