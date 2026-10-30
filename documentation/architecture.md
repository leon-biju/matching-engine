# Project Architecture
For both versions the planned command flow is having a arbitrary `Command Source` that feeds in commands to the `Matching Engine` and produces outputs to an `Event Sink`.

These sources and sinks can range from file readers to network streams.
An SPSC queue will be used for feeding data to/from source and sink.

The command source maps symbol names to `SymbolId`s before commands reach the engine, so the engine only ever works with IDs.

## Version 2
The Version 2 design (threading model, ownership of order IDs across threads, and keeping batches in engine-sequence order) is deliberately not planned yet. It will be designed once the reference engine and baseline benchmarks are in place. Open questions are tracked in the [TODO](../TODO.md).
