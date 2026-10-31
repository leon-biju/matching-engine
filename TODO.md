# TODO
Notes and open decisions to come back to. See the [Roadmap](documentation/roadmap.md) for the phases.

## Phase 2: reference engine
- [x] **Randomised invariant tests.** Run random command streams through the engine and check after every command that the book is never crossed, quantities balance, and the resting-order index agrees with the book. Write the stream generator so it can be reused as the Phase 3 workload generator.

## Phase 3: workload and baseline benchmarks
- [x] Workload file format for recorded command streams. Fixed-size binary records behind a small header. The harness loads the whole file into memory before timing, so the format never affects the measurements.
- [x] Workload generator. The `realistic` and `aggressive` benchmark profiles, and `make_workload`, which writes a workload file from a profile, seed and command count.
- [x] Benchmark harness. A custom harness, not Google Benchmark. It measures the engine on its own, with no SPSC queue, and reports throughput and per-command latency percentiles (p50, p99, p99.9) from separate passes.
- [x] `release` preset for benchmarks: `CMAKE_BUILD_TYPE=Release` with `-march=native`. Tests keep running under `default` and `sanitize`.
- [x] Baseline benchmarks of the reference engine, recorded in `documentation/benchmarks.md` with the CPU, compiler, seed and profile for each run. CI builds the harness but never checks timings, because shared runners are too noisy.

## Phase 4: optimised engine
The decisions behind these tasks are in [Architecture](documentation/architecture.md#version-2).

- [x] **Sink interface.** Engines push events to an `EventSink` instead of returning a new `EventBatch` for every command.
- [ ] **Differential test.** Run Version 1 and every other engine side by side on generated streams (default config, `realistic` and `aggressive`) and compare every batch with `EXPECT_EQ`, reporting the first batch that differs.
- [ ] **Profile Version 1** on the `realistic` and `aggressive` workloads to find where the time goes, and record the findings to guide the Version 2 design.
- [ ] **Version 2 design.** Choose the data structures from the profile and write them into `architecture.md`.
- [ ] **Version 2 engine.** Build it, validating it with the differential test as it develops, and add it to `tests/engines.hpp` and `engine_benchmark`.
- [ ] **SPSC queue and full-pipeline benchmark.** Source, engine and sink on their own threads. Measure throughput, and latency from queuing a command to its batch reaching the sink. Document the method in `benchmarks.md`.
- [ ] Once Version 2 exists, add a brief comparison to the README that links to `benchmarks.md`.

## Open decisions
- **Engine construction.** The `Engine` concept assumes an engine can be built from the symbol count alone. If Version 2 needs more configuration, add an `EngineConfig` struct with defaults and build every engine from it, or give Version 2 defaults and keep the current construction in `tests/engines.hpp`.

## Later
- **Splitting symbols across matching threads.** Only if profiling shows matching itself is the bottleneck. It needs a stream interface and a semantics change; see [Architecture](documentation/architecture.md#threading) for why.
