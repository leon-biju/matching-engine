# TODO
Notes and open decisions to come back to. See the [Roadmap](documentation/roadmap.md) for the phases.

## Phase 2: reference engine
- [x] **Randomised invariant tests.** Run random command streams through the engine and check after every command that the book is never crossed, quantities balance, and the resting-order index agrees with the book. Write the stream generator so it can be reused as the Phase 3 workload generator.

## Phase 3: workload and baseline benchmarks
- [ ] Workload file format for recorded command streams.
- [ ] Workload generator.
- [ ] Baseline benchmarks of the reference engine.

## Open decisions
- **Version 2 threading model.** Sharding by symbol is the obvious option, but two things cross shards: order IDs are global (the `DuplicateOrderId` check, and cancels that name only the ID), and batches must come out in global engine-sequence order.
- **Workload format.** Binary or text.
- **What to measure.** Throughput only, or per-command latency percentiles too. Google Benchmark or a custom harness.
- **SPSC queue in benchmarks.** Whether benchmarks include the queue or measure the engine on its own.
- **Version 2 construction.** The `Engine` concept assumes an engine can be built from the symbol count alone. If Version 2 needs more configuration (e.g. a thread count), give it defaults or change the shared construction in `tests/engines.hpp`.
