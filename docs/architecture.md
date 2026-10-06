# Architecture

The components will be arranged to flow as follows:

```text
            Command Source
                   |
                   v
            MatchingEngine
                   |
          +--------+--------+
          |        |        |
          v        v        v
       AAPL      MSFT      NVDA
       Book      Book      Book
          |        |        |
          +--------+--------+
                   |
                   v
              Event Sink
```

## Components

* **Command source:** Provides commands to the engine.
* **MatchingEngine:** Assigns global sequence numbers, routes commands to symbol books, and publishes completed event batches.
* **SymbolBook:** Owns matching logic and the bid/ask price levels for one symbol.
* **OrderMap:** Looks up active orders by ID for cancellation. Also tracks previously accepted IDs separately from active order references.
* **Event sink:** Receives output event batches.

Components should be swappable and testable independently, with shared libraries for tests and common functionality across the two versions. The command source and event sink keep transport separate from matching logic.

## Implementation approaches

For V1 (Reference matching engine):

* Use STL-based symbol books, e.g. `std::map` for bids and asks.
* Use straightforward allocation and let the STL objects handle it using heap allocation.
* Prioritise readability, correctness and determinism.

For V2 (Performance optimised engine):

* Partition work across threads while preserving the same engine contract.
* Use profiling to decide whether custom storage, allocation, or cache-line aware structures are needed.

## Concurrency and ordering

The engine entry point is the serialisation point for assigning global sequence numbers. Each symbol book has just one logical owner, with no concurrent book mutation. V1 uses just one thread for the entire engine. V2 can partition symbols across worker threads.

Each worker returns a complete event batch for a command. The engine buffers completed batches as needed to meet the ordering contract in [Command and event interfaces](interfaces.md#event-ordering).
