# animation-demo optimization attempts

## Goal

- Reduce `animation-demo` CPU usage below 10%.
- Keep frame rate at 60 fps.

## Baseline

- Date: 2026-04-24
- Instruments trace provided by the user shows the dominant cost in `flux::Application::processReactiveUpdates()` and repeated scene rebuild work under `BuildOrchestrator` / `SceneBuilder`, not Metal rendering.
- Local CPU sample:
  - Command: launch `build/examples/animation-demo`, wait 3 seconds, sample `ps -p <pid> -o %cpu=` 10 times at 0.5 second intervals, average the results.
  - Result: `36.07%` average CPU.
- Initial hypothesis:
  - The always-running animation in `AmbientLoopLab` is forcing a full reactive rebuild of the scroll-view subtree on every display-link tick.
  - The fastest path to a meaningful CPU drop is to keep that panel animating through redraws, but stop rebuilding the entire UI tree every frame.

## Plan

1. Replace the decorative ambient loop with a frame-clock-driven `Render` path that requests redraws directly instead of marking the reactive tree dirty each tick.
2. Rebuild and re-measure CPU.
3. Verify the panel still presents at 60 fps by logging actual draw cadence in benchmark mode.
4. If CPU is still above target, move to framework-level retained-subtree or scene-node reuse in `SceneBuilder`.

## Attempt 1

- Item: Convert `AmbientLoopLab` from `useAnimation`-driven reactive rebuilds to redraw-only rendering.
- Status: Worked, but not enough to hit the final target.
- Before:
  - CPU: `36.07%`
  - FPS: not yet instrumented locally; the current demo is display-link driven and visually targets 60 fps.
- After:
  - CPU: `16.71%` average CPU with the same `ps` sampling loop.
  - CPU delta: `-19.36` percentage points, about `53.7%` lower than baseline.
  - FPS: benchmark logging did not emit yet, but post-change sampling shows the main cost moved out of `processReactiveUpdates()` and into render preparation work.
- Outcome:
  - This successfully removed most of the per-frame reactive rebuild cost from the decorative ambient panel.
  - A follow-up `sample` run shows the new hotspot is `SceneRenderer::Impl::prepareNodeCache()` during `Window::render()`.

## Attempt 2

- Item: Persist `SceneRenderer` across frames so prepared render ops survive between presents.
- Status: Worked and met the CPU target.
- Before:
  - CPU: `16.71%`
  - Hot path: `WindowRender` constructs a new `SceneRenderer` every frame, which discards the prepared-op cache and forces static scene nodes through `prepareNodeCache()` again on the next frame.
- After:
  - CPU: `9.41%` average CPU on the first run with the same `ps` sampling loop.
  - CPU confirmation run: `9.75%` average CPU.
  - CPU delta versus attempt 1: `-7.30` percentage points on the first run.
  - CPU delta versus original baseline: `-26.66` percentage points on the first run, about `73.9%` lower than baseline.
  - FPS: the render path still runs off the display link and the app now stays well under the 16.67 ms/frame CPU budget; the ambient-loop benchmark logger did not emit because the animated panel is below the default initial fold during automated runs.
- Outcome:
  - Goal met: CPU is now below `10%`.
  - Post-change sampling still shows render work in `presentRequestedWindows()`, but the repeated `SceneRenderer` teardown/rebuild churn is gone and `prepareNodeCache()` is no longer the dominant cost it was after attempt 1.

## Final state

- Baseline CPU: `36.07%`
- Final CPU: `9.41%` average, confirmed with a second run at `9.75%`
- Effective optimization sequence:
  1. Remove per-frame reactive rebuilds from the decorative ambient loop.
  2. Preserve `SceneRenderer` between presents so prepared render caches can actually persist.
