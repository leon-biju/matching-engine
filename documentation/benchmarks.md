# Benchmarks
How the engines are benchmarked, and the results. Each result names the machine, compiler and workload it came from, because numbers from different machines can't be compared.

## Method
`engine_benchmark` replays a workload file through every engine. It loads the whole file and builds each engine before timing starts, so only `process()` is measured. It measures the engine on its own, with no SPSC queue.

It runs two passes over each engine:
* **Throughput:** the whole workload through a fresh engine, repeated (5 times by default). It reports the best and median commands per second, and the event count, which must match between engines.
* **Latency:** the whole workload once through a fresh engine, timing each command with `std::chrono::steady_clock`. It reports the p50, p90, p99 and p99.9 in nanoseconds, plus the maximum. The clock reads add a few tens of nanoseconds to each time. The maximum mostly shows the OS interrupting the run, not the engine.

Workloads come from `make_workload`, which uses a generator profile (see [Architecture](architecture.md#command-generator)) and a seed.

## Running
Build with the `release` preset (see [Building and testing](../README.md#building-and-testing)), then:

```sh
build-release/benchmarks/make_workload realistic 1 5000000 realistic.workload
build-release/benchmarks/engine_benchmark realistic.workload
```

`make_workload` takes a profile (`realistic`, `aggressive` or `test`), a seed, a command count and an output file. `engine_benchmark` takes the workload file and an optional repeat count.

CI runs both tools on a small workload as a smoke test, but never checks timings, because shared runners are too noisy.

## Results

### Version 1 baseline
Machine: Intel Core i5-1135G7 laptop (4 cores, 8 threads), with frequency scaling on. Compiler: Clang 23.1.4, `release` preset. Measured on 2026-10-31. Each workload has 5,000,000 commands from seed 1.

| Workload | Throughput, best | Throughput, median | Events | p50 | p90 | p99 | p99.9 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `realistic` | 13.03M/s | 12.97M/s | 8,154,116 | 83 ns | 130 ns | 205 ns | 456 ns |
| `aggressive` | 10.42M/s | 10.32M/s | 10,550,327 | 91 ns | 149 ns | 241 ns | 405 ns |
