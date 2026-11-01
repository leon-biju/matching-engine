# Architecture
For both versions, an arbitrary command source feeds commands to the matching engine, which sends its output to an event sink. Sources and sinks can range from file readers to network streams. [SPSC queues](#pipeline) carry data from the source to the engine and from the engine to the sink. See [Interfaces](interfaces.md) for what passes through them.

The command source maps symbol names to `SymbolId`s before commands reach the engine, so the engine only ever works with IDs.

## Engines
Both engines satisfy the `matching::Engine` concept in [engine.hpp](../include/matching/core/engine.hpp): they are built from a symbol count, and send exactly one event batch to an [event sink](interfaces.md#event-batch) for each command.

* **Version 1** (`matching::version1::ReferenceEngine`): one `OrderBook` per symbol. Each side of a book is a `std::map` of price levels, and each level is a `std::list` FIFO queue, so a resting order can be removed by its handle.
* **Version 2:** not built yet. See [Version 2](#version-2) for the decisions made so far.

## Pipeline
The headers in [include/matching/pipeline/](../include/matching/pipeline) join the stages' threads. They work with any engine.

* **SPSC queue** (`matching::pipeline::SpscQueue`): a bounded, lock-free ring buffer for one producer thread and one consumer thread. Its capacity is a power of two. The producer publishes each slot with a release store of its index, and the consumer reads the index with an acquire load, so everything the producer wrote before a push is visible after the matching pop. The two indices sit on separate 64-byte cache lines. `try_push` and `try_pop` never wait. `push` and `pop` spin while the queue is full or empty, pausing the CPU for the first few tries and then yielding the thread, so a stage cannot starve the others on a machine with fewer cores than threads.
* **Queue sink** (`matching::pipeline::QueueSink`): an [event sink](interfaces.md#event-batch) that writes each call into an SPSC queue of `SinkItem`s, waiting while the queue is full. A batch crosses the queue as a `BatchBegin` holding its sequence, then its events in order, then a `BatchEnd`. The engine writes one batch at a time, so the consumer sees batches whole and in sequence order. On the sink thread, `replay` turns each item back into the matching call on any other event sink.

The command queue carries `EngineCommand` values. The engine thread pops each command and processes it into a `QueueSink`.

## Tests
The correctness suite in [engine_correctness_test.cpp](../tests/engine_correctness_test.cpp) runs against every engine listed in [engines.hpp](../tests/engines.hpp), so adding an engine there runs every behaviour test against it. Each test also runs with both a buy and a sell aggressor, with prices mirrored for the sell side.

The invariant suite in [engine_invariant_test.cpp](../tests/engine_invariant_test.cpp) runs long random command streams through the same engines. After every command it checks the batch against a shadow model of the books built only from the events: sequencing, the event patterns, quantity balance, price-time priority at the resting price, FOK fills, cancels, and that no book is ever crossed. Each stream has a fixed seed in the test name, so a failure can be rerun with `--gtest_filter='*_seed<N>'`.

The differential suite in [engine_differential_test.cpp](../tests/engine_differential_test.cpp) runs every engine side by side with `version1::ReferenceEngine` on the same generated command streams, and compares each command's batch with the reference's. It stops at the first batch that differs and reports the command's index, the command and both batches. It runs the default generator config and the `realistic` and `aggressive` profiles, each over several fixed seeds named in the test name, so a failure can be rerun with `--gtest_filter='*_<profile>_seed<N>'`. Version 1 is also compared with itself, which checks that it is deterministic.

The shared tests run each command through `test::process` in [engines.hpp](../tests/engines.hpp), which fails if an engine breaks the [sink protocol](interfaces.md#event-batch).

## Command generator
`matching::workload::CommandGenerator` in [command_generator.hpp](../include/matching/workload/command_generator.hpp) produces a random command stream from a seed and a `GeneratorConfig`. The same seed and config give the same stream on any platform. It mixes all order types and sides with invalid symbols, zero quantities, bad prices, reused IDs, and cancels for both resting and unknown orders. The invariant tests use it with the default config.

The `realistic` and `aggressive` configs in `matching::workload::profiles` are for benchmarks. They send only well-formed commands with unique IDs. `realistic` prices most GTC orders on their own side of the mid price so they rest, and holds back cancels until 20,000 GTC orders are waiting to be named. Its books settle at about 1,350 resting orders over 130 price levels per symbol, with heavy cancelling. `aggressive` keeps prices narrow so most orders trade.

## Workload files
A workload file records a command stream and the symbol count to build the engine with, so a benchmark can replay the same stream. [workload_file.hpp](../include/matching/workload/workload_file.hpp) defines the format and reads and writes it. Each command is a fixed-size little-endian binary record, and malformed commands are kept as they are. A benchmark loads the whole file before timing starts, so the format never affects the measurements.

## Benchmarks
The tools in [benchmarks/](../benchmarks) are:
* `make_workload`, which writes a workload file from a generator profile and seed.
* `engine_benchmark`, which replays a file through every engine on one thread and reports throughput and latency.
* `pipeline_benchmark`, which replays a file through every engine as a full [pipeline](#pipeline), with the source, engine and sink on their own threads, and reports throughput and end-to-end latency.

See [Benchmarks](benchmarks.md) for the method and results.

## Version 2
These decisions were made before building Version 2. The tasks that follow from them, and the questions still open, are in the [TODO](../TODO.md#phase-4-optimised-engine).

### Threading
Version 2 matches on a single thread, like Version 1. Its speed comes from faster data structures, not from splitting the matching across threads. The threads are the stages around it: the command source, the engine and the event sink each run on their own thread, joined by the [SPSC queues](#pipeline).

Splitting symbols across several matching threads was rejected for now. The thread that routes commands and the stage that restores engine-sequence order would both be single-threaded, each handoff between threads costs about as much as matching a command in Version 1, and global order IDs clash with the rules. A market order rejected with `NoLiquidity` frees its ID for reuse, and whether that happens depends on the book, so the routing thread could not check `DuplicateOrderId` without waiting for the symbol's thread. It may be tried later if profiling shows matching itself is the bottleneck, and it would need a change to [Semantics](semantics.md#validation).

### Engine interface
Engines send events to a sink instead of returning them: `process(command, sink)` pushes each event the command causes to a sink the caller provides (see [Event batch](interfaces.md#event-batch)). The tests collect events into batches, `engine_benchmark` only counts them, and the [queue sink](#pipeline) writes them straight into the SPSC queue, so an engine never has to allocate a new event vector for every command. Version 1 still builds each command's events in a vector, but reuses it between commands. Each call handles one command completely before returning. Splitting symbols across threads would need an interface that takes a whole stream of commands, so that waits until the split is attempted.

### Validation
Version 1 is the reference: Version 2 is correct when it produces the same event batches as Version 1 for the same commands. The [differential suite](#tests) checks this for every engine in the tests, and the correctness and invariant suites run against every engine as well.

### Benchmarks
`engine_benchmark` keeps measuring each engine on its own, with no queues. `pipeline_benchmark` runs the source, engine and sink on their own threads with SPSC queues between them. It timestamps each command as it is queued and again when its batch reaches the sink, so it measures what the threading is for. Both use the same workload files, so their results can be compared. See [Benchmarks](benchmarks.md#method) for how each one measures.
