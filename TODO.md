# TODO
Notes and open decisions to come back to. See the [Roadmap](documentation/roadmap.md) for the phases.

## Phase 2: reference engine
- [x] **Randomised invariant tests.** Run random command streams through the engine and check after every command that the book is never crossed, quantities balance, and the resting-order index agrees with the book. Write the stream generator so it can be reused as the Phase 3 workload generator.

## Phase 3: workload and baseline benchmarks
- [x] Workload file format for recorded command streams. Fixed-size binary records behind a small header. The harness loads the whole file into memory before timing, so the format never affects the measurements.
- [ ] Workload generator. The `realistic` and `aggressive` benchmark profiles are done. Still needed: a tool that writes a workload file from a seed, profile and command count.
- [ ] Benchmark harness. A custom harness, not Google Benchmark. It measures the engine on its own, with no SPSC queue, and reports throughput and per-command latency percentiles (p50, p99, p99.9) from separate passes.
- [x] `release` preset for benchmarks: `CMAKE_BUILD_TYPE=Release` with `-march=native`. Tests keep running under `default` and `sanitize`.
- [ ] Baseline benchmarks of the reference engine, recorded in `documentation/benchmarks.md` with the CPU, compiler, seed and profile for each run. CI builds the harness but never checks timings, because shared runners are too noisy.
- [ ] Once Version 2 exists, add a brief comparison to the README that links to `benchmarks.md`.

## Open decisions
- **Version 2 threading model.** Sharding by symbol is the obvious option, but two things cross shards: order IDs are global (the `DuplicateOrderId` check, and cancels that name only the ID), and batches must come out in global engine-sequence order.
- **Version 2 construction.** The `Engine` concept assumes an engine can be built from the symbol count alone. If Version 2 needs more configuration (e.g. a thread count), give it defaults or change the shared construction in `tests/engines.hpp`.
