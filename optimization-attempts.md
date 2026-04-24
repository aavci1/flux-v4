# animation-demo optimization attempts

## Goal

- Reduce `animation-demo` CPU usage below 10%.
- Keep frame rate at 60 fps.
- Keep `AmbientLoopLab` on `useAnimation`; demo-side removal is not acceptable.

## Measurement

- Date: 2026-04-24
- CPU method:
  - Launch `build/examples/animation-demo`
  - Wait 3 seconds
  - Sample `ps -p <pid> -o %cpu=` 10 times at 0.5 second intervals
  - Average the 10 samples
- Stack traces use `/usr/bin/sample <pid> 1 1`.

## Baselines

- Original baseline before optimization work:
  - CPU: `36.07%`
  - Primary hot path: `flux::Application::processReactiveUpdates()` and repeated `BuildOrchestrator` / `SceneBuilder` work.
- Current framework-only baseline after restoring `useAnimation` in `AmbientLoopLab`:
  - CPU: `32.47%`
  - Notes:
    - This keeps the previously committed framework change that reuses `SceneRenderer` across frames.
    - The demo-side redraw-only ambient-loop workaround was reverted and is not part of the accepted path.

## Attempt 1

- Item: Convert `AmbientLoopLab` from `useAnimation`-driven rebuilds to redraw-only rendering.
- Type: demo-specific
- Status: Numerically effective, but rejected and reverted.
- Before:
  - CPU: `36.07%`
- After:
  - CPU: `16.71%`
  - Delta: `-19.36` percentage points
- Outcome:
  - This removed most reactive rebuild work, but it changed the demo instead of fixing the framework.
  - The user explicitly rejected this approach, so it is not part of the accepted optimization path.

## Attempt 2

- Item: Persist `SceneRenderer` across frames so prepared render ops survive between presents.
- Type: framework
- Status: Worked, but not enough by itself once `useAnimation` was restored.
- Before:
  - CPU: `36.07%`
- After:
  - CPU: `32.47%`
  - Delta: `-3.60` percentage points
- Outcome:
  - This remains a valid framework optimization and stays in the codebase.
  - It reduces render-preparation churn, but the dominant cost is still reactive rebuilds from `useAnimation`.

## Attempt 3

- Item: Retained-subtree reuse through `SceneBuilder` plus layout-child wrapper nodes.
- Type: framework
- Status: Failed, reverted.
- Before:
  - CPU: `32.47%`
- After:
  - CPU: `35.97%`
  - Delta: `+3.50` percentage points
- Outcome:
  - The retained path only helped when the subtree was already stable at the composite-body level.
  - The hot `animation-demo` path is still dominated by regular layout elements (`ScrollView`, `VStack`, `HStack`) rebuilding under the dirty animated component.
  - The extra wrapper/allocation overhead outweighed the limited reuse, so the attempt was reverted.

## Attempt 4

- Item: Skip `useAnimation`-driven composite dirties when the observing component is outside the window viewport.
- Type: framework
- Status: Failed, reverted.
- Before:
  - CPU: `32.47%`
- After:
  - CPU: `34.64%`
  - Delta: `+2.17` percentage points
- Outcome:
  - The fresh stack trace still shows `flux::Application::processReactiveUpdates()` rebuilding the same `ScrollView` / `VStack` path on nearly every tick.
  - Visibility gating did not suppress the dominant rebuild path in this demo, so it was reverted.

## Attempt 5

- Item: Generic scene-node reuse for structurally unchanged elements, including regular layout children inside dirty animated components.
- Type: framework
- Status: Failed, reverted.
- Before:
  - CPU: `32.47%`
- After:
  - CPU: `36.23%`
  - Delta: `+3.76` percentage points
- Outcome:
  - This widened reuse beyond retained composite bodies, but it introduced too much bookkeeping and wrapper overhead.
  - The benchmark regressed, so the change was reverted.

## Attempt 6

- Item: Re-key source-element outer measurement to the logical component path instead of the nested scene path.
- Type: framework
- Status: Failed, reverted.
- Before:
  - CPU: `32.47%`
- After:
  - CPU: `35.31%`
  - Delta: `+2.84` percentage points
- Outcome:
  - The second per-frame dirty key turned out to be a measure-only `AmbientLoopLab` state with no scene snapshot or scene node.
  - Changing the source-element measurement key did not remove that duplicate dirty state and did not activate the partial rebuild path, so the change was reverted.

## Attempt 7

- Item: Let incremental rebuild ignore measure-only dirty keys and rebuild the single scene-backed animated subtree.
- Type: framework
- Status: Worked, committed.
- Before:
  - CPU: `32.47%`
- After:
  - CPU: `17.13%`
  - Delta: `-15.34` percentage points
- Outcome:
  - The dirty set for the ambient animation contained one real scene-backed key plus one measure-only duplicate key with no snapshot or node.
  - Filtering the incremental path down to dirty keys that actually have a recorded build snapshot and scene node activated subtree rebuilds and removed the full-root rebuild cost.

## Current read

- Accepted framework change still in place:
  - `SceneRenderer` reuse across frames.
- Newly accepted framework change:
  - Incremental rebuild now ignores measure-only dirty keys and rebuilds the real scene-backed animated subtree.
- Current blocker:
  - The dominant rebuild cost is now `SceneBuilder::buildSubtree(...)` for the animated card rather than a full-root rebuild.
  - Rendering is still a large second cost because the rebuilt subtree gets recreated every frame and the renderer has to prepare/draw it again.
- Next direction:
  - Reuse existing scene nodes inside the partial subtree rebuild so stable descendants survive between animation ticks instead of being recreated each frame.
