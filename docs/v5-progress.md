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

- [ ] `Element` concept interface slimmed.
- [ ] Reactive `Bindable<T>` modifier storage and overloads added.
- [ ] Body-rerun infrastructure removed or parked as specified.
- [ ] Surviving library/tests compile.
- [ ] Surviving tests pass.
- [ ] Forbidden body-rerun symbols are absent from `src/`, `include/`, and surviving tests.

Gate status: pending.

## Stage 4 — Mount Root + Scope Tree

- [ ] `MountContext` and `MountRoot` implemented.
- [ ] `Element::mount` implemented for required primitives/composites/containers.
- [ ] `Runtime` wired to mount once and drain effects pre-layout.
- [ ] `tests/MountRootTests.cpp` passes.
- [ ] `examples/hello-world` builds and renders.
- [ ] Mount/unmount leak checks pass.

Gate status: pending.

## Stage 5 — Hooks Rewrite

- [ ] `useState`, `useComputed`, `useEffect`, `useAnimation`, `useEnvironment`, input hooks, and action hooks restored with once-at-mount semantics.
- [ ] Animation writes through the reactive graph.
- [ ] Environment lookup returns reactive values.
- [ ] Restored tests pass.
- [ ] `reactive-demo`, `animation-demo`, and `theme-demo` build and render.
- [ ] AmbientLoopLab CPU is below the documented target and recorded.

Gate status: pending.

## Stage 6 — Reactive Control Flow

- [ ] `For` implemented.
- [ ] `Show` implemented.
- [ ] `Switch` implemented.
- [ ] Control-flow tests pass.
- [ ] `scroll-demo` and `lambda-studio` build and run.
- [ ] `For` preserves row scope state across reorder and disposes removed scopes.

Gate status: pending.

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
