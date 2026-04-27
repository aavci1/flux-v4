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
| 9 - Cleanup, validation, release prep | Code and docs passed; release promotion deferred pending post-cutover hardening | pending |
| 9.5 - Post-cutover hardening | In progress; see below | multiple |

## Stage 9 Status

- The public reactive namespace is `Flux/Reactive`.
- Reactive tests live under `tests/Reactive`.
- The test target is `flux_reactive_tests`.
- The project version is `5.0.0`.
- Public docs have been updated for retained mounting, scopes, bindings, control flow, and reactive environment values.
- Final performance data is recorded in [v5-final-perf.md](v5-final-perf.md).
- The earlier `ps`-based warm-idle example sweep and `/usr/bin/sample` AmbientLoopLab rows are no longer treated as final perf data; final reactive perf uses `FLUX_PROFILE_REACTIVE=ON` deterministic wall-clock instrumentation.
- Main-branch promotion and release tags require explicit approval.

## Stage 9.5 Post-cutover Hardening

Completed so far:

- Relayout architecture: retained-tree resize relayout, prepared render-op replay at current positions, retained stack sizing fixes, text-position preservation, scale binding survival across relayout, and bounded reactive size propagation.
- Ownership and retained-build correctness: composite body measurement effects scoped, composite bodies materialized once, `useState` body-once invariant documented/asserted, child mount contexts split into shared/owned scope paths.
- Reactivity cleanup: legacy `observe` callbacks removed, computed dirty propagation restored, transition/effect interaction isolated, and deferred diamond-poll revisit TODO documented with profiling rationale.
- Control-flow/layout fixes: collapsed control flow remains mounted for layout updates, For row measurement is retained/cached, stale `SceneBuilder`, `MeasureLayoutCache`, cursor-controller, text-edit-behavior, leaf-bounds, and grid-layout artifacts removed.
- Input/action cutover: focus, keyboard-focus, hover, and press hooks wired to runtime interaction signals; view/window action hooks wired to the registry with scope cleanup.
- Environment hard cutover: type-keyed `std::any` environment layers were replaced with compile-time environment keys, flat keyed bindings, keyed `.environment<Key>(...)` modifiers, and `useEnvironment<Key>()` / `useEnvironmentReactive<Key>()` helpers.
- Performance hardening: scene-graph plumbing callbacks moved to `SmallFn`, unchanged binding/text effects short-circuit, redundant redraw arming is coalesced, animation subscribers are no longer cloned per frame, keyed environment lookups avoid per-fire replay, and deterministic `FLUX_PROFILE_REACTIVE=ON` AmbientLoopLab data is recorded.
- Release-prep scan: `scripts/check_stale_symbols.sh` now checks removed SceneBuilder artifacts, unlisted implementation sources, declaration-only public headers, and unmatched forward declarations.

Remaining release-prep work is tracked by the v5 action items list, including any follow-up perf target from the latest AmbientLoopLab sample.

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
- `scripts/check_stale_symbols.sh` passed.
