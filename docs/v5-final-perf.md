# Flux v5 Final Performance Record

Date: 2026-04-27

## Summary

| Build | Scenario | Result |
|-------|----------|--------|
| Historical baseline | Ambient animation lab | 36% CPU |
| Optimized pre-cutover baseline | Ambient animation lab | 10% CPU |
| v5 Stage 9 | Full tests and examples | Green normal, ASAN, UBSAN, and TSAN validation |
| v5 Stage 10 | `animation-demo` AmbientLoopLab running | Reactive implementation: 2.7% of active display-link samples |

## Measurement Method

Stage 10 launched `build/examples/animation-demo`, waited 3 seconds for startup work to settle, then sampled the live process for 30 seconds with `/usr/bin/sample <pid> 30 5 -mayDie`. AmbientLoopLab's `AnimationClock::subscribe` loop was active during the run. The active frame denominator was the display-link callback path (`displayLinkDidFire`, 446 samples). Reactive implementation time counted `flux::Reactive::*` propagation and scheduling frames, including allocator time below `EffectState::onDirty`, but excluding user/UI work invoked by effect bodies such as text layout, environment snapshots, and redraw requests. The measured reactive implementation share was `12 / 446 = 2.7%`, which clears the 5% gate.

For context, the broader reactive-triggered inclusive path under `BatchGuard` was `35 / 446 = 7.8%`. That number includes user/UI effect work; if future perf work targets the inclusive path, the largest visible costs in this sample were text layout and environment snapshot copying during bindable updates.

## Final Validation

- Normal build included tests, examples, and benchmarks.
- Normal `ctest` passed.
- All 28 normal example binaries passed launch smoke checks.
- ASAN full build, `ctest`, and all 28 example launch smokes passed.
- UBSAN test build and `ctest` passed.
- TSAN test build and `ctest` passed after the Metal present completion handler stopped using a by-reference block capture for the frame semaphore.
