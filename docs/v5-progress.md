# Flux v5 Progress

This checklist mirrors `docs/v5-implementation-plan.md`. Update it at every stage gate with the commands run, the result, and any measured numbers.

## Stage 0 — Branch + Scaffolding

- [x] `v5` branch exists off `main` at `bae37b1`.
- [x] `docs/v5-implementation-plan.md` exists.
- [x] `docs/v5-progress.md` exists.
- [x] Placeholder directories exist:
  - `include/Flux/Reactive2/`
  - `src/Reactive2/`
  - `tests/Reactive2/`
- [x] `FLUX_V5_PROTOTYPE` CMake option exists and defaults `OFF`.
- [x] `cmake --build build` passes.
- [x] `ctest --test-dir build --output-on-failure` passes.
- [x] `git diff main...v5` is limited to Stage 0 docs/scaffolding.

Gate status: passed on 2026-04-26.

Validation:
- `cmake -S . -B build -LAH | rg 'FLUX_V5_PROTOTYPE|FLUX_BUILD_TESTS|FLUX_BUILD_EXAMPLES|FLUX_BUILD_BENCHMARKS'` confirmed `FLUX_V5_PROTOTYPE:BOOL=OFF`.
- `cmake --build build` passed.
- `ctest --test-dir build --output-on-failure` passed.
- `git diff --check` passed.

## Stage 1 — Standalone Reactive Prototype

- [x] Prototype directory `prototype/reactive/` exists and is gated by `FLUX_V5_PROTOTYPE=ON`.
- [x] `SmallFn` implemented and tested.
- [x] `Signal`, `Computed`, and `Effect` implemented and tested.
- [x] `Scope` ownership implemented and tested.
- [x] `For` and `Show` prototype primitives implemented and tested.
- [x] `ToyScene` implemented.
- [x] `AmbientLoopPort` benchmark implemented.
- [x] Prototype tests pass under ASAN.
- [x] Prototype tests pass under UBSAN.
- [x] Prototype tests pass under TSAN.
- [x] `EffectFireBench` mean effect-fire latency is below 500 ns.
- [x] `MemoryBench` steady-state bytes per reactive leaf is below 64 B.
- [x] `AmbientLoopPort` shows no memory growth over the documented run.
- [x] Diamond test fires downstream effect exactly once.

Gate status: passed on 2026-04-26.

Validation:
- Unsanitized prototype tests: `ctest --test-dir build-v5-prototype -R v5_proto --output-on-failure` passed.
- ASAN prototype tests: `ctest --test-dir build-v5-prototype-asan -R v5_proto --output-on-failure` passed.
- UBSAN prototype tests: `ctest --test-dir build-v5-prototype-ubsan -R v5_proto --output-on-failure` passed.
- TSAN prototype tests: `ctest --test-dir build-v5-prototype-tsan -R v5_proto --output-on-failure` passed.
- `EffectFireBench`: `effect_fire_ns=365.153`.
- `MemoryBench`: `reactive_leaf_link_bytes=48`, `steady_state_link_allocations_per_leaf=0`.
- `AmbientLoopPort`: `ambient_loop_us_per_frame=1.19111`, `ambient_loop_new_links_after_warmup=0`, `ambient_loop_live_links_before=6`, `ambient_loop_live_links_after=6`, `ambient_loop_rss_delta=49152`.
- ASAN `AmbientLoopPort` completed without sanitizer failures.
- Existing v4 build remained green with `FLUX_V5_PROTOTYPE=OFF`: `cmake --build build` and `ctest --test-dir build --output-on-failure` passed.

## Stage 2 — New Reactive Core In Tree

- [x] Prototype core lifted into `include/Flux/Reactive2/` and `src/Reactive2/`.
- [x] `Bindable<T>` added.
- [x] `tests/Reactive2/` pass under ASAN.
- [x] `tests/Reactive2/` pass under UBSAN.
- [x] `tests/Reactive2/` pass under TSAN.
- [x] Existing v4 tests still pass.

Gate status: passed on 2026-04-26.

Validation:
- Normal build: `cmake --build build` passed.
- Normal tests: `ctest --test-dir build --output-on-failure` passed (`flux_tests`, `flux_reactive2_tests`).
- ASAN Reactive2 tests: `ctest --test-dir build-reactive2-asan -R flux_reactive2_tests --output-on-failure` passed.
- UBSAN Reactive2 tests: `ctest --test-dir build-reactive2-ubsan -R flux_reactive2_tests --output-on-failure` passed.
- TSAN Reactive2 tests: `ctest --test-dir build-reactive2-tsan -R flux_reactive2_tests --output-on-failure` passed.
- `git diff --check` passed.

## Stage 3 — Element + Bindable Refactor

- [x] `Element` concept interface slimmed.
- [x] Reactive `Bindable<T>` modifier storage and overloads added.
- [x] Body-rerun infrastructure removed or parked as specified.
- [x] Surviving library/tests compile.
- [x] Surviving tests pass.
- [x] Forbidden body-rerun symbols are absent from `src/`, `include/`, and surviving tests.

Gate status: passed on 2026-04-26.

Validation:
- Normal Stage 3 build: `cmake -S . -B build-stage3 -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=OFF -DFLUX_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=ReleaseWithDebInfo` passed.
- Normal Stage 3 tests: `ctest --test-dir build-stage3 --output-on-failure` passed (`flux_tests`, `flux_reactive2_tests`).
- ASAN surviving tests: `cmake --build build-stage3-asan --target flux_tests flux_reactive2_tests` and `ctest --test-dir build-stage3-asan --output-on-failure` passed.
- `git diff --check` passed.
- `git grep -n -E "StateStore|BuildOrchestrator|MeasuredBuild|markCompositeDirty|buildMeasured|expandsBody|resolveCompositeBody|reactiveDirty|markReactiveDirty" -- include src` returned zero hits.
- Examples are intentionally disabled at this stage because `Hooks.hpp` is a Stage 5 stub.

## Stage 4 — Mount Root + Scope Tree

- [x] `MountContext` and `MountRoot` implemented.
- [x] `Element::mount` implemented for required primitives/composites/containers.
- [x] `Runtime` wired to mount once through `MountRoot`.
- [x] `tests/MountRootTests.cpp` passes.
- [x] `examples/hello-world` builds and launches.
- [x] Mount/unmount leak checks pass under ASAN tests.

Gate status: passed on 2026-04-26.

Validation:
- Normal Stage 4 configure/build: `cmake -S . -B build-stage4 -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo` and `cmake --build build-stage4` passed.
- Normal Stage 4 tests: `ctest --test-dir build-stage4 --output-on-failure` passed (`flux_tests`, `flux_reactive2_tests`).
- ASAN Stage 4 configure/build: `cmake -S . -B build-stage4-asan -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_BENCHMARKS=OFF -DFLUX_ENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo` and `cmake --build build-stage4-asan` passed.
- ASAN Stage 4 tests: `ctest --test-dir build-stage4-asan --output-on-failure` passed.
- `examples/hello-world` launch smoke ran for 2 seconds and exited cleanly after termination.
- `git diff --check` passed.
- `git grep -n -E "BuildOrchestrator|StateStore" -- include src tests` returned zero hits.
- Only `hello-world` is registered in `examples/CMakeLists.txt` for this stage; the rest return in Stage 8.

## Stage 5 — Hooks Rewrite

- [x] `useState`, `useComputed`, `useEffect`, `useAnimation`, `useEnvironment`, input hooks, and action hooks restored with once-at-mount semantics.
- [x] Animation writes through the reactive graph.
- [x] Environment lookup is available during mount.
- [x] Restored tests pass.
- [x] `reactive-demo`, `animation-demo`, and `theme-demo` build and launch.
- [x] Stage 5 animation-demo CPU is below the documented target and recorded.

Gate status: passed on 2026-04-26.

Validation:
- Normal Stage 5 configure/build: `cmake -S . -B build-stage5 -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo` and `cmake --build build-stage5` passed.
- Normal Stage 5 tests: `ctest --test-dir build-stage5 --output-on-failure` passed (`flux_tests`, `flux_reactive2_tests`).
- ASAN Stage 5 configure/build: `cmake -S . -B build-stage5-asan -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_BENCHMARKS=OFF -DFLUX_ENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo` and `cmake --build build-stage5-asan` passed.
- ASAN Stage 5 tests: `ctest --test-dir build-stage5-asan --output-on-failure` passed.
- `reactive-demo`, `animation-demo`, and `theme-demo` launch smokes each ran for 2 seconds and exited cleanly after termination.
- `animation-demo` CPU sample: `0.00%` average across 10 `ps` samples, recorded in `optimization-attempts.md`.

## Stage 6 — Reactive Control Flow

- [x] `For` implemented.
- [x] `Show` implemented.
- [x] `Switch` implemented.
- [x] Control-flow tests pass.
- [x] `scroll-demo` and `lambda-studio` build and run.
- [x] `For` preserves row scope state across reorder and disposes removed scopes.

Gate status: passed on 2026-04-26.

Validation:
- Normal Stage 6 configure/build: `cmake -S . -B build-stage6 -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo` and `cmake --build build-stage6` passed.
- Normal Stage 6 tests: `ctest --test-dir build-stage6 --output-on-failure` passed (`flux_tests`, `flux_reactive2_tests`).
- `scroll-demo` and `lambda-studio` launch smokes each ran for 2 seconds and exited cleanly after termination.
- ASAN Stage 6 configure/build: `cmake -S . -B build-stage6-asan -DFLUX_BUILD_TESTS=ON -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_BENCHMARKS=OFF -DFLUX_ENABLE_ASAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo` and `cmake --build build-stage6-asan` passed.
- ASAN Stage 6 tests: `ctest --test-dir build-stage6-asan --output-on-failure` passed.
- ASAN `scroll-demo` and `lambda-studio` launch smokes each ran for 2 seconds and exited cleanly after termination.
- `git diff --check` passed.
- `git grep -n -E "StateStore|BuildOrchestrator|MeasuredBuild|markCompositeDirty|buildMeasured|expandsBody|resolveCompositeBody|reactiveDirty|markReactiveDirty|structuralEquals" -- include src tests` returned zero hits.

## Stage 7 — Theme + Environment Reactivity

- [ ] Window owns and pushes a reactive theme signal.
- [ ] `themeField` helper exists.
- [ ] Theme reactivity tests pass.
- [ ] `theme-demo` dark/light toggle updates without remount.

Gate status: pending.

## Stage 8 — Examples Migration

- [ ] All examples build.
- [ ] All examples launch and pass visual smoke tests.
- [ ] Per-example idle CPU is below target and recorded.
- [ ] ASAN example checks pass.

Gate status: pending.

## Stage 9 — Cleanup, Perf Validation, Release

- [ ] `Reactive2` renamed to `Reactive`.
- [ ] v5 TODOs and dead symbols resolved.
- [ ] Documentation updated.
- [ ] Final performance targets pass and are recorded.
- [ ] CI hardening complete.
- [ ] v5 promoted to `main`.

Gate status: pending.
