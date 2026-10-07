# matching-engine

A C++23 project to build and compare two multi-symbol order matching engines: a clear reference implementation and a performance-optimised implementation. Both will process streams of orders and produce deterministic results under the same matching rules.

## Project scope

- **Version 1 (reference engine):** A single-threaded correctness baseline that prioritises clarity and provides a reference for validating Version 2.
- **Version 2 (optimised engine):** A high-performance, multi-threaded implementation evaluated against the reference engine for both correctness and speed.

## Building and Testing

Requires CMake 3.25 or newer and any C++23 compiler (Developed primarily using clang++).

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## Continuous integration

GitHub Actions runs on pushes and PRs to/from `master` branch:

- Release builds and GoogleTest tests with both Clang 18 and GCC 14 on Ubuntu 24.04.
- A Clang Debug build with AddressSanitizer (including leak detection) and UndefinedBehaviorSanitizer.

The workflow is defined in [.github/workflows/ci.yml](.github/workflows/ci.yml).

## Documentation

- [Matching semantics](docs/semantics.md): Order model and matching rules.
- [Command and event interfaces](docs/interfaces.md): Inputs, outputs, and event ordering. Together with matching semantics, this defines the engine contract.
- [Architecture](docs/architecture.md): Engine structure and implementation approaches for both versions.
- [Testing](docs/testing.md): Validation strategy and correctness invariants.
- [Benchmarking](docs/benchmarking.md): Performance metrics and workloads.
- [Roadmap](docs/roadmap.md): Development phases and open decisions.

## License

[MIT License](LICENSE)
