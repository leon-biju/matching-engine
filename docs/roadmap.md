# Roadmap

The development phases are:

1. Specification and architecture.
2. Reference engine and correctness tests.
3. Workload generator and baseline benchmarks.
4. Profiling and optimised engine, with differential validation as it develops.
5. Further optimisation based on profiling results.

Networking / an end-to-end system is an optional follow-up.

## Open decisions to be made...

* Should unknown symbols be rejected, or should a new symbol book be created automatically?? The current [matching rules](semantics.md#command-validation) require configured symbols and reject unknown ones.
