# Benchmarking

Compare both versions using the same command streams, starting books, and hardware. Record the build settings and worker count with each result.

## Metrics

* Throughput in input commands per second.
* Command latency: p50, p90, p99, p99.9, p99.99, and max. Measure from the engine entry point until the complete event batch is published to the sink.

For profiling, also measure cycles, instructions, branch misses, cache misses, and allocations per input command. One command can produce several events, so use input commands as the common unit when comparing the engines.

## Workloads

* Just one hot symbol or many different symbols.
* Deep or shallow books.
* Cancel-heavy, match-heavy, or mostly-resting traffic.
* Steady or bursty traffic.
