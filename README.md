# matching-engine
A C++23 project to build and compare two multi-symbol order matching engines: a clear reference implementation and a performance-optimised implementation. Both will process streams of orders and produce deterministic results under the same matching rules.

## Project scope
- **Version 1 (reference engine):** A single-threaded correctness baseline that prioritises clarity and provides a reference for validating Version 2.
- **Version 2 (optimised engine):** A high-performance, multi-threaded implementation evaluated against the reference engine for both correctness and speed.

## Building and testing
Requires CMake 3.25 or newer, a C++23 compiler and Make. The default preset selects `clang++`. Test configuration requires GoogleTest.

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

To select another compiler on the first configure, use `cmake --preset default -DCMAKE_CXX_COMPILER=g++`.

## Documentation
* Semantics on how both engines should behave are in [Semantics](documentation/semantics.md).

## License
[MIT License](LICENSE)