# AGENTS.md

C++23 order matching engines: V1 is a single-threaded reference engine (current work), and V2 is a future optimised engine that must produce identical output. See [README.md](README.md) and [documentation/](documentation/). Phases are in the [roadmap](documentation/roadmap.md), and open tasks and decisions are in [TODO.md](TODO.md).

## Build and test

See [Building and testing](README.md#building-and-testing). All tests must pass under both the `default` and `sanitize` presets before a change is done. CI also checks this with GCC 14 and Clang 18, so don't use language or library features those compilers lack.

## Rules

- [semantics.md](documentation/semantics.md) and [interfaces.md](documentation/interfaces.md) define the engine contract. If code and docs disagree, ask rather than silently changing either.
- Every engine satisfies the `matching::Engine` concept in [engine.hpp](include/matching/core/engine.hpp).
- The command source maps symbol names to `SymbolId`s; engines never handle symbol names.
- Do not add third-party dependencies without asking.
- Run `clang-format -i` on changed C++ files. CI fails on any unformatted file, checked with `clang-format-18`.
- V1 favours readable STL over cleverness; save optimisation for V2.
- Don't commit. Stage related changes as one commit and let the user review and commit them.

## Tests

- Behaviour tests go in [engine_correctness_test.cpp](tests/engine_correctness_test.cpp) and run against every engine listed in [engines.hpp](tests/engines.hpp). Tests of one engine's internals go in a separate file for that engine.
- Assert on whole event batches with `EXPECT_EQ`, not individual fields.
- Parameterise book tests over `OrderSide` so both aggressors are covered.
- Check that failed operations leave the book unchanged.

## Documentation

- British English, sentence-case headings, short declarative sentences.
- Don't hard-wrap prose; keep each paragraph or bullet on one line and let the viewer wrap it.
- Each fact has one home; link to it (with section anchors) rather than repeating it.
- Update docs in the same change as the code: `architecture.md` for components and [TODO.md](TODO.md) for open decisions.
