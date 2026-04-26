# Flux v5 Final Performance Record

Date: 2026-04-26

## Summary

| Build | Scenario | Result |
|-------|----------|--------|
| Historical baseline | Ambient animation lab | 36% CPU |
| Optimized pre-cutover baseline | Ambient animation lab | 10% CPU |
| v5 Stage 5 | `animation-demo` warm idle | 0.00% CPU average |
| v5 Stage 8 | 28 example warm idle sweep | 0.00% CPU average for every example |
| v5 Stage 9 | Full tests and examples | Green normal, ASAN, UBSAN, and TSAN validation |

## Measurement Method

Stage 8 launched each example, waited 2 seconds for startup work to settle, sampled `ps -p <pid> -o %cpu=` five times at 0.4 second intervals, then terminated the process. Exit code `143` was treated as a clean smoke termination.

## Example Idle Results

All current examples measured `0.00%` average warm idle CPU:

`alert-demo`, `animation-demo`, `blend-demo`, `button-demo`, `card-demo`, `checkbox-demo`, `cursor-demo`, `hello-world`, `icon-demo`, `image-demo`, `lambda-studio`, `layout-demo`, `markdown-formatter-demo`, `popover-demo`, `reactive-demo`, `scene-graph-demo`, `scroll-demo`, `segmented-demo`, `select-demo`, `slider-demo`, `table-demo`, `text-demo`, `textinput-demo`, `theme-demo`, `toast-demo`, `toggle-demo`, `tooltip-demo`, `typography-demo`.

Detailed raw entries are preserved in `optimization-attempts.md`.

## Final Validation

- Normal build included tests, examples, and benchmarks.
- Normal `ctest` passed.
- All 28 normal example binaries passed launch smoke checks.
- ASAN full build, `ctest`, and all 28 example launch smokes passed.
- UBSAN test build and `ctest` passed.
- TSAN test build and `ctest` passed after the Metal present completion handler stopped using a by-reference block capture for the frame semaphore.
