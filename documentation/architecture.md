# Architecture
For both versions, an arbitrary command source feeds commands to the matching engine, which sends its output to an event sink. Sources and sinks can range from file readers to network streams. SPSC queues carry data from the source to the engine and from the engine to the sink. See [Interfaces](interfaces.md) for what passes through them.

The command source maps symbol names to `SymbolId`s before commands reach the engine, so the engine only ever works with IDs.

## Engines
Both engines satisfy the `matching::Engine` concept in [engine.hpp](../include/matching/core/engine.hpp): they are built from a symbol count, and turn each command into exactly one event batch.

* **Version 1** (`matching::version1::ReferenceEngine`): one `OrderBook` per symbol. Each side of a book is a `std::map` of price levels, and each level is a `std::list` FIFO queue, so a resting order can be removed by its handle.
* **Version 2:** not designed yet (see below).

## Tests
The correctness suite in [engine_correctness_test.cpp](../tests/engine_correctness_test.cpp) runs against every engine listed in [engines.hpp](../tests/engines.hpp), so adding an engine there runs every behaviour test against it.

## Version 2
The Version 2 design (threading model, ownership of order IDs across threads, and keeping batches in engine-sequence order) is deliberately not planned yet. It will be designed once the reference engine and baseline benchmarks are in place. Open questions are tracked in the [TODO](../TODO.md).
