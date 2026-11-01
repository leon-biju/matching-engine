# Benchmarks
How the engines are benchmarked, and the results. Each result names the machine, compiler and workload it came from, because numbers from different machines can't be compared.

## Method
There are two benchmark tools. `engine_benchmark` measures each engine on its own, and `pipeline_benchmark` measures it inside the full threaded pipeline. Both replay the same workload files, which come from `make_workload`. It uses a generator profile (see [Architecture](architecture.md#command-generator)) and a seed.

### Engine benchmark
`engine_benchmark` replays a workload file through every engine. It loads the whole file and builds each engine before timing starts, so only `process()` is measured. It measures the engine on its own, with no SPSC queue.

It runs two passes over each engine:
* **Throughput:** the whole workload through a fresh engine, repeated (5 times by default). It reports the best and median commands per second, and the event count, which must match between engines.
* **Latency:** the whole workload once through a fresh engine, timing each command with `std::chrono::steady_clock`. It reports the p50, p90, p99 and p99.9 in nanoseconds, plus the maximum. The clock reads add a few tens of nanoseconds to each time. The maximum mostly shows the OS interrupting the run, not the engine.

### Pipeline benchmark
`pipeline_benchmark` replays a workload file through every engine as a full [pipeline](architecture.md#pipeline). A source thread pushes the commands into the command queue, the engine thread processes them into a `QueueSink`, and a sink thread reads the events back. It loads the whole file before timing starts. Each run builds a fresh engine and fresh queues, starts the three threads, and only then releases them, so starting threads is not timed. The threads are not pinned to cores.

It runs two passes over each engine:
* **Throughput:** the source pushes the whole workload as fast as the queues accept it, repeated (5 times by default). Each run is timed from releasing the threads to the sink receiving the last batch. It reports the best and median commands per second, and the event count, which must match `engine_benchmark`.
* **Latency:** one run, with the source paced to queue commands evenly at half the median throughput from the first pass. The source records the time just before it queues each command, and the sink reads the clock when it receives that command's `end_batch`. It reports the difference as p50, p90, p99 and p99.9 in nanoseconds, plus the maximum.

The latency is end to end. It includes the time a command waits in the command queue, the matching, and the time its events wait in the event queue, as well as the cost of each handoff between threads. A source that runs flat out keeps the command queue full, so the latency would mostly measure how long the queue is. Pacing at half the throughput keeps the queues mostly empty, so the latency shows the handoffs and the matching, plus the queueing when a burst of slow commands builds a backlog. The pace is fixed at half so that runs on different machines and engines are loaded alike.

The sink also checks that batches arrive in sequence order, and the tool fails if they do not.

## Running
Build with the `release` preset (see [Building and testing](../README.md#building-and-testing)), then:

```sh
build-release/benchmarks/make_workload realistic 1 5000000 realistic.workload
build-release/benchmarks/engine_benchmark realistic.workload
build-release/benchmarks/pipeline_benchmark realistic.workload
```

`make_workload` takes a profile (`realistic`, `aggressive` or `test`), a seed, a command count and an output file. `engine_benchmark` and `pipeline_benchmark` take the workload file and an optional repeat count.

`pipeline_benchmark` needs three cores to itself, or the threads spin while they wait for each other and its results mean little. Close other busy programs while it runs.

CI runs every tool on a small workload as a smoke test, but never checks timings, because shared runners are too noisy.

## Results

### Version 1 baseline
Machine: Intel Core i5-1135G7 laptop (4 cores, 8 threads), with frequency scaling on. Compiler: Clang 23.1.4, `release` preset. Measured on 2026-10-31. Each workload has 5,000,000 commands from seed 1.

| Workload | Throughput, best | Throughput, median | Events | p50 | p90 | p99 | p99.9 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `realistic` | 13.03M/s | 12.97M/s | 8,154,116 | 83 ns | 130 ns | 205 ns | 456 ns |
| `aggressive` | 10.42M/s | 10.32M/s | 10,550,327 | 91 ns | 149 ns | 241 ns | 405 ns |
