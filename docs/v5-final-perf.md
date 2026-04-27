# Flux v5 Final Performance Record

Date: 2026-04-27

## Summary

| Build | Scenario | Result |
|-------|----------|--------|
| Historical baseline | Ambient animation lab | 36% CPU |
| Optimized pre-cutover baseline | Ambient animation lab | 10% CPU |
| v5 Stage 9 | Full tests and examples | Green normal, ASAN, UBSAN, and TSAN validation |
| v5 final | `animation-demo` AmbientLoopLab @60fps, 55s steady-state | Reactive scheduling/propagation: 0.02% wall-clock; inclusive reactive-triggered effect path: 0.21% wall-clock |

## Measurement Method

The final run uses deterministic instrumentation instead of `/usr/bin/sample`. The profile build was configured with:

```bash
cmake -S . -B build-profile -DCMAKE_BUILD_TYPE=Release -DFLUX_BUILD_EXAMPLES=ON -DFLUX_BUILD_TESTS=ON -DFLUX_PROFILE_REACTIVE=ON
cmake --build build-profile --target animation-demo flux_tests -j8
```

`FLUX_PROFILE_REACTIVE=ON` compiles in exclusive `mach_absolute_time` timers at:

- `SignalState::set`
- `EffectState::run`
- `Computation::pollSourcesChanged`
- `Observable::propagatePending`
- `flushEffects`
- the display-link frame boundary

Each display-link report uses wall-clock time as the denominator, not active sample counts. Nested timers subtract child time before accumulating, so the bucket sum is stable and comparable between runs.

AmbientLoopLab was run for 60 seconds from `build-profile/examples/animation-demo` after the final API, animation, reactive-core, and environment-binding waves landed. The first 5-second report was discarded as warmup. The next eleven reports, from 10s through 60s, each contained 300 frames, confirming a sustained 60fps animation. Their averaged bucket percentages were:

| Bucket | Wall-clock share |
|--------|------------------|
| Signal set | 0.01% |
| Effect runs, including effect-body work | 0.19% |
| Poll | below 0.005% |
| Propagation | below 0.005% |
| Flush | below 0.005% |
| Inclusive total | 0.21% |

The reactive scheduling/propagation subtotal excludes effect-body work and is approximately `signal_set + poll + propagation + flush = 0.02%`. The inclusive reactive-triggered path includes effect-body work and is 0.21%. The previous Stage 10, 10b, and 10c `/usr/bin/sample` rows were removed because their active-sample denominators varied too much to compare directly.

## Final Validation

- Normal build included tests, examples, and benchmarks.
- Normal `ctest` passed.
- All 28 normal example binaries passed launch smoke checks.
- ASAN full build, `ctest`, and all 28 example launch smokes passed.
- UBSAN test build and `ctest` passed.
- TSAN test build and `ctest` passed after the Metal present completion handler stopped using a by-reference block capture for the frame semaphore.
