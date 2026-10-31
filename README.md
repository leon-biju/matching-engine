# matching-engine
A C++23 project to build and compare two multi-symbol order matching engines: a clear reference implementation and a performance-optimised implementation. Both will process streams of orders and produce deterministic results under the same matching rules.

## Project scope
- **Version 1 (reference engine):** A single-threaded correctness baseline that prioritises clarity and provides a reference for validating Version 2.
- **Version 2 (optimised engine):** A high-performance, multi-threaded implementation evaluated against the reference engine for both correctness and speed.

## Building and testing
Requires CMake 3.25 or newer, GCC 14 or Clang 18 (or newer) and Make. The default preset selects `clang++`. Test configuration requires GoogleTest.

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

To select another compiler on the first configure, use `cmake --preset default -DCMAKE_CXX_COMPILER=g++`.

To build and test with AddressSanitizer and UndefinedBehaviorSanitizer, use the `sanitize` preset in place of `default` for all three commands. It builds in `build-sanitize`.

For benchmarks, use the `release` preset to configure and build. It builds in `build-release` with optimisations and `-march=native`, so its binaries are tuned to the machine that builds them.

[CI](.github/workflows/ci.yml) runs on pushes to `master` and on pull requests. It builds and tests with GCC 14 and Clang 18 on Ubuntu 24.04 under both presets, and checks formatting with `clang-format-18`.

## Documentation
* [Semantics](documentation/semantics.md): the order model and matching rules both engines follow.
* [Interfaces](documentation/interfaces.md): the commands and events that pass in and out of an engine.
* [Architecture](documentation/architecture.md): how the components fit together.
* [Roadmap](documentation/roadmap.md): the development phases.
* [TODO](TODO.md): open tasks and decisions.

## License
[MIT License](LICENSE)