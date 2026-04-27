# Flux v5 Progress

This file records the stage-gate status for the v5 cutover.

## Completed Stages

| Stage | Result | Commit |
|-------|--------|--------|
| 0 - Branch and scaffolding | Passed | `05bba33f` |
| 1 - Standalone reactive prototype | Passed | `db016b5d` |
| 2 - New reactive core in tree | Passed | `86dec3e7` |
| 3 - Element and bindable refactor | Passed | `d589402a` |
| 4 - Mount root and scope tree | Passed | `6a4bcc60` |
| 5 - Hooks rewrite | Passed | `124b8fcd` |
| 6 - Reactive control flow | Passed | `19ae410d` |
| 7 - Theme and environment reactivity | Passed | `62ab68e5` |
| 8 - Examples migration | Migration passed; sample-based per-example perf pending | `b7d0c20e` |
| 9 - Cleanup, validation, release prep | Passed through code/doc validation; release promotion awaits approval | pending |

## Stage 9 Status

- The public reactive namespace is `Flux/Reactive`.
- Reactive tests live under `tests/Reactive`.
- The test target is `flux_reactive_tests`.
- The project version is `5.0.0`.
- Public docs have been updated for retained mounting, scopes, bindings, control flow, and reactive environment values.
- Final performance data is recorded in [v5-final-perf.md](v5-final-perf.md).
- The earlier `ps`-based warm-idle example sweep is no longer treated as final perf data; any per-example perf table should use the same `/usr/bin/sample` methodology as the AmbientLoopLab measurement.
- Main-branch promotion and release tags require explicit approval.

## Latest Validation

Normal Stage 9 validation:

```bash
cmake -S . -B build-stage9 -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-stage9
ctest --test-dir build-stage9 --output-on-failure
```

Final Stage 9 validation:

```bash
cmake -S . -B build-stage9 -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-stage9
ctest --test-dir build-stage9 --output-on-failure
```

Result: passed.

Additional validation:

- All 28 normal example binaries launch-smoked and exited cleanly after termination.
- `build-stage9-asan`: full build, `ctest`, and all 28 ASAN example launch smokes passed.
- `build-stage9-ubsan`: test build and `ctest` passed.
- `build-stage9-tsan`: test build and `ctest` passed.
- `git diff --check` passed.
- The final stale-symbol scan over source, tests, examples, docs, README, and CMake returned zero hits.
