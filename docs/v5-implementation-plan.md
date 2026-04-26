# Flux v5 — Implementation Plan

**Branch:** `v5` (single long-lived branch off `main` at `bae37b1`).
**Strategy:** Hard cutover. No backwards-compat. v4 stays on `main`, frozen except for security fixes. v5 develops in isolation; when stages 0–9 are green, `v5` becomes `main` and v4 is tagged `v4-final`.
**Staging logic:** Each stage compiles, ships its own tests, and produces a runnable artifact (a unit-test binary, an internal demo, or a ported example). You can stop at any stage boundary and still have something working. **Stages are gates, not deadlines** — gate criteria are concrete and measurable; if a gate fails, the stage is not done regardless of how many days have passed.

This plan supersedes all prior v5 design notes for tactical purposes. Open architectural questions still live in the design discussion; what's locked in below is what will actually be coded.

## 0. Locked architectural decisions

These are not up for re-debate during implementation. If one of them is wrong, the discovery happens in the prototype (Stage 1) and the decision is revised there, before it spreads.

### Reactive core
- **Algorithm:** push-pull with three-state coloring (clean / check / dirty). Two-phase write: `set()` propagates `Pending` only; `Dirty` is resolved lazily by `checkDirty()` on the next read. This is the alien-signals / Reactively model.
- **Graph storage:** intrusive doubly-linked-list dependency edges. Each `Link` sits in two lists: source→subscribers and subscriber→sources. State is a `std::uint16_t` bit-flag (`Mutable | Watching | Recursed | Dirty | Pending | Disposed`).
- **Allocation:** `Signal<T>`, `Computed<T>`, `Effect` are heap-allocated nodes accessed via `shared_ptr`-like value-semantic handles. Handles are cheap to copy and pass. *Not* arena-allocated — Leptos's arenas solve a Rust-specific problem we don't have.
- **Closure storage:** hand-rolled SBO callable (32 bytes inline, heap fallback). Not `std::function`. ~100 lines, lives at `include/Flux/Reactive/Detail/SmallFn.hpp`.
- **Tracking:** `thread_local Computation* sCurrentObserver` for source registration; `thread_local Owner* sCurrentOwner` for ownership.
- **Equality:** writes that don't change the value (`==` returns true) do not propagate. Already true of v4 `Signal::set` when `T` is equality-comparable; preserved.
- **Out-of-tracking reads:** silent in release; `assert` in debug builds when reading from a function annotated `[[reactive]]`. Provide `peek()` for explicit unsubscribed reads and `untrack(lambda)` for region-scoped opt-out.
- **Async:** out of scope for v5.0. Tracking ends at the first `co_await` or any blocking call, same as every other framework. Document loudly. Revisit in v5.1.

### Ownership
- `Scope` is a public type and an RAII guard. `Scope` owns child computations and a list of `onCleanup` callbacks. Destructor disposes children depth-first, runs cleanups in reverse order, and tears down subscriptions.
- Implicit ownership via thread-local `sCurrentOwner` is the default. Explicit ownership via `withOwner(scope, lambda)` is available for advanced use.
- Owner tree is **separate from** the dependency graph. They share lifetimes but not edges. This is the Solid/Leptos consensus.

### View tree shape
- **Build-once components.** A component's `body()` runs exactly once at mount. Mounting establishes the structural tree; signals at the leaves drive updates. This is locked.
- **Two-tree model:** transient `Element` tree (returned from `body()`) → retained `SceneNode` tree (lives across frames). Mount walks the Element tree once, materializes scene nodes, and installs reactive bindings (Effects) on each scene node. The Element tree is then discarded.
- **No three-tree.** Flutter's separate Element-identity layer is unnecessary in our model because `SceneNode` already plays that role and v4 already invested heavily in scene-node reuse and prepared-op caching.
- **Reactive primitives for control flow:** `For`, `Show`, `Switch`. Each owns its own subtree and child Scope. When the predicate/key changes, the owned subtree disposes and a new subtree mounts — the *parent* never re-runs.
- **No more `body()` reruns of any kind.** No partial rebuilds, no `markCompositeDirty`. The entire incremental-rebuild apparatus (`StateStore::pendingDirtyComponents`, `lastBody`, `lastBodyEpoch`, `bodyStructurallyStable`, etc.) is deleted, not refactored.

### DSL
- **`Bindable<T>`** is the single primitive for reactive leaf properties. Constructible from `T` or from any `Fn() -> T`. Detection happens at compile time via concept (`std::is_invocable_r_v<T, F>`).
- **`Element` modifiers** all accept `Bindable<T>` instead of `T` for properties that are valid as reactive (size, opacity, fill, stroke, position, padding, cornerRadius, transforms). Structural fields (children, alignment for stacks, key, focusable) stay value-typed for v5.0.
- **Component values stay aggregate-init-friendly.** `Text{.text = "...", .font = Font::body()}` still works because `Bindable<std::string>` is constructible from `std::string`. `Text{.text = [=]{ return ...; }}` Just Works.
- **No new external DSL.** No macros for component definition. No clang plugin. Standard C++23 toolchain.

### Hooks
- `useState<T>(initial)` returns `Signal<T>`. Constructed once at mount. Not slot-table-keyed. Allocated as a child of the current Scope.
- `useComputed<T>(fn)` returns `Computed<T>`. Same lifetime semantics.
- `useEffect(fn)` returns nothing; runs once at mount, re-runs on tracked-source changes, disposes on unmount.
- `useAnimation<T>(initial, options)` returns `Animation<T>` (which exposes a `Signal<T>`-shaped read interface). Same once-at-mount semantics.
- `useEnvironment<T>()` returns `Signal<T>` (or whatever reactive shape T provides — typically `Signal<Theme>` for the theme case).
- `useMemo` is removed. It was a slot-table optimization for body-rerun semantics; with build-once, plain local `const` variables in `body()` do the job.
- `useOuterElementModifiers` is removed. Modifiers attach to elements, not to component bodies, in v5.
- The thread-local `StateStore::current()` machinery is replaced by the thread-local `Owner` stack. `useState` outside a build pass is a debug-build error — same as v4.
- `useFocus`/`useHover`/`usePress` return `Signal<bool>`. Underlying machinery (FocusController/HoverController/GestureTracker) is preserved but rewired to set signals instead of marking composites dirty.

### Theme reactivity
- `useEnvironment<Theme>()` returns `Signal<Theme>`. Whole-theme signal. Coarse on paper, fine in practice (theme changes are rare).
- For granular subscription, a `themeField(&Theme::space3)` helper returns `Computed<float>` that reads only that field through the theme signal. Components that care about a single field opt in.
- No structural remount on theme change. The whole point of v5 is to never structurally remount on a value change.

### Migration
- Hard cutover. No `LegacyView` wrapper. No parallel runtime.
- Examples are ported in dependency order (Stage 8). Each ported example is a regression test for that stage.
- Tests are migrated alongside the systems they test (Stages 2–6), not as a final pass.

### What's removed entirely
- `StateStore` and the entire slot-table mechanism (~620 lines of `.cpp` plus the header).
- `BuildOrchestrator` (~270 lines) — replaced by a much smaller `MountRoot` (~150 lines target).
- `ComponentBuildContext`, `ComponentBuildResult`, `MeasuredBuild`, `CompositeBodyResolution` — all the body-rerun plumbing.
- `useMemo`, `useOuterElementModifiers`.
- `Element::structuralEquals`, `Element::valueEquals` (no diffing).
- `lastBody`, `lastBodyEpoch`, `bodyStructurallyStable`, `lastBuildSnapshot`, `reusableMeasures`, `lastSceneElement`, `pendingEnvironmentDependencies`, `discardCurrentRebuildBody` and the rest of the per-component rebuild state.
- `Element::Concept::resolveCompositeBody`, `Element::Concept::buildMeasured`, `Element::expandsBody`, `MeasuredComponent`/`ExpandsBodyComponent` concepts. The `body()` contract changes: `body()` is called once and returns an Element (or a list of them); there is no more measure-vs-build duality at the composite level.
- `notifyObserverList`'s composite-dirty path (`StateStore::markCompositeDirty`).
- `signalBridgeMarkReactiveDirty` and `scheduleReactiveRebuildAfterAnimationChange` — the per-write whole-tree-rebuild kicker. Replaced by a per-effect direct write to scene nodes.
- `Element::measureId_` and the measure-cache invalidation infrastructure tied to body-rerun.

### What's preserved
- `SceneNode` and the scene graph (`include/Flux/SceneGraph/`, `src/SceneGraph/`). Including the `preparedRenderOps_` cache, dirty bits, and node reuse — these all become *more* effective in v5 because mount-time-only structure means scene nodes live longer.
- `Element` as a type-erased view-value wrapper, but slimmed: drops the build/measure machinery, keeps fill/stroke/size/etc. modifiers (now over `Bindable<T>`).
- Layout system (`src/UI/Layout/`). v5 still does measure/arrange the same way.
- All graphics infrastructure (`src/Graphics/`): TextSystem, Canvas, Font, Path, Color.
- All input infrastructure (`InputDispatcher`, `FocusController`, `HoverController`, `GestureTracker`) — rewired internally but API-compatible.
- Theme structure (~80 fields) — unchanged; only access pattern changes.
- `Animation<T>`, `Transition`, `WithTransition`, animation clock — already push-based with observer model; integrates into v5 reactive graph as a third Observable type.
- Examples directory structure. `optimization-attempts.md`. `docs/conventions.md` aesthetic rules.

## 1. Stage map (10 stages)

| Stage | Name | Approx. LOC delta | Gate |
|---|---|---|---|
| 0 | Branch + scaffolding | ±0 | `v5` branch builds v4 unchanged |
| 1 | Standalone reactive prototype | +1.5K (separate dir) | AmbientLoopLab port runs at <500 ns/effect, <64 B/leaf |
| 2 | New reactive core in tree | +1.2K, −0.3K | All reactive-core tests pass |
| 3 | Element + Bindable refactor | +0.8K, −2.0K | Element tests pass; no body-rerun callsites remain |
| 4 | Mount root + Scope tree | +0.4K, −1.8K | hello-world renders |
| 5 | Hooks rewrite | +0.3K, −0.8K | reactive-demo, animation-demo, theme-demo render |
| 6 | Reactive control flow (For/Show/Switch) | +0.3K | scroll-demo, lambda-studio render |
| 7 | Theme + environment reactivity | +0.2K | theme-demo light/dark toggle does not remount |
| 8 | Examples migration | ±0 (no framework code) | All 31 examples build and run |
| 9 | Cleanup, perf validation, release | −0.5K (dead code removal) | Frame-budget targets met; v5 → main |

Net code delta (rough): **−2,000 lines** despite adding the new reactive runtime, because v4's slot-table + body-rerun + structural-comparison machinery is much larger than the replacement.

The expanded per-stage breakdown follows.

## 2. Stage 0 — Branch + scaffolding

**Goal:** A `v5` branch that still builds v4 cleanly, with new directories ready to receive code, and the work-tracking docs in place.

**Steps:**
1. `git checkout -b v5` from `main`.
2. Create new directories (empty placeholders): `include/Flux/Reactive2/`, `src/Reactive2/`, `tests/Reactive2/`. The `2` suffix is temporary — these get renamed to drop it in Stage 9.
3. Add a `docs/v5-implementation-plan.md` (this file) and `docs/v5-progress.md` (a checklist mirroring the stages, updated at every gate).
4. Add `FLUX_V5_PROTOTYPE` CMake option (defaults `OFF`). It will gate the prototype build in Stage 1.
5. CI: existing v4 tests must continue to run on the `v5` branch unchanged. No regressions allowed at this stage.

**Gate:** `cmake --build build` produces the v4 binaries; `ctest` is green; `git diff main...v5` shows only the new directories and docs.

**Output artifact:** none yet.

## 3. Stage 1 — Standalone reactive prototype

**Goal:** Validate the reactive-core architecture in isolation, free from Flux's existing types and CMake graph. The prototype's job is to find the design bugs *before* they spread into the rest of the codebase.

**Location:** `prototype/reactive/` at the repo root, gated by `FLUX_V5_PROTOTYPE=ON`. Standalone CMakeLists, no dependency on the rest of Flux.

**Files to create (estimates):**
```
prototype/reactive/
├── CMakeLists.txt
├── include/
│   ├── SmallFn.hpp           ~100 lines (SBO callable)
│   ├── Signal.hpp            ~150 lines
│   ├── Computed.hpp          ~120 lines
│   ├── Effect.hpp            ~100 lines
│   ├── Scope.hpp             ~120 lines
│   ├── For.hpp               ~150 lines
│   ├── Show.hpp              ~50  lines
│   └── ToyScene.hpp          ~80  lines (mock SceneNode for ergonomics testing)
├── tests/
│   ├── SignalTests.cpp       ~200 lines
│   ├── ComputedTests.cpp     ~150 lines
│   ├── EffectTests.cpp       ~200 lines
│   ├── ScopeTests.cpp        ~150 lines
│   ├── ForShowTests.cpp      ~150 lines
│   └── DiamondTests.cpp      ~100 lines (glitch-freedom)
└── benchmarks/
    ├── EffectFireBench.cpp   measures ns/effect
    ├── MemoryBench.cpp       measures bytes/leaf at steady state
    └── AmbientLoopPort.cpp   the canonical animation case
```

**Implementation order within Stage 1:**

1. **SmallFn.** Function-pointer + small object buffer. Test against `std::function` for size and call cost. ~100 lines.
2. **Signal + Computed + Effect, single-threaded, no Scope yet.** Just the dependency graph: read registers, write propagates. Three-state coloring. Two-phase write. Tests: simple read/write, transitive computeds, dynamic deps (conditional reads), the diamond case.
3. **Scope.** Owner tree, RAII, `onCleanup`. Test that `Scope` destruction tears down child computeds + effects + their subscriptions cleanly. Test the `withOwner(scope, lambda)` explicit form.
4. **For + Show.** User-supplied keys for `For`; reuse-by-key reconciliation. Each `<For>` row is a child Scope of the `<For>` itself. Tests: insert, remove, reorder, mass-replace, and the "row body does not re-run when item value mutates internally" invariant.
5. **ToyScene.** A trivial `Node` with `setSize(float, float)`, `setOpacity(float)`, `addChild`, `removeChild`. Stand-in for `SceneNode` so the prototype can be developed without pulling in the graphics stack.
6. **AmbientLoopPort benchmark.** Reproduce the AmbientLoopLab structure: 5 reactive bars, an animated `phase` signal driven from an animation-clock mock, the conditional reduced-motion branch (driving via `<Show>`). Run a 60-fps simulation in a tight loop. Measure: nanoseconds per effect fire, bytes per reactive leaf at steady state, total CPU spent in the reactive layer per simulated frame.

**Gate (hard, no exceptions):**
- All prototype tests pass under ASAN/UBSAN/TSAN.
- `EffectFireBench` shows mean effect-fire latency < 500 ns. If it's > 500 ns, the closure storage or dispatch path needs rework before Stage 2.
- `MemoryBench` shows steady-state bytes-per-reactive-leaf < 64. If it's > 64, the node layout needs to be revisited.
- `AmbientLoopPort` running at simulated 60 fps for 30 seconds shows zero memory growth (ASAN no leaks; RSS stable to ±100 KB).
- The diamond test (A → B, A → C, both → D, write A) shows D fires exactly once. No glitches.

**If the gate fails:** revise the architecture in the prototype. Do not move to Stage 2.

**Output artifact:** standalone test + benchmark binaries. Numbers recorded in `docs/v5-progress.md`.

## 4. Stage 2 — New reactive core in-tree

**Goal:** Lift the prototype's reactive core into the Flux tree under `Flux/Reactive2`, replacing nothing yet. Existing `Flux/Reactive` (Signal, Computed, Animation) stays untouched.

**Why a parallel namespace temporarily:** so the rest of v4 keeps compiling while the new core is being integrated. By end of Stage 5 the old `Flux/Reactive` is deleted and `Reactive2` is renamed.

**Files to create:**
```
include/Flux/Reactive2/
├── SmallFn.hpp           (lifted from prototype)
├── Signal.hpp
├── Computed.hpp
├── Effect.hpp
├── Bindable.hpp          ~80 lines (variant<T, SmallFn<T()>>)
├── Scope.hpp
├── Untrack.hpp           ~30 lines (peek + untrack helpers)
└── Detail/
    ├── Link.hpp          intrusive list edge node
    ├── ObserverStack.hpp thread_local current observer
    └── Flags.hpp         bit-flag enum + helpers
src/Reactive2/
├── Signal.cpp
├── Computed.cpp
├── Effect.cpp
├── Scope.cpp
└── ObserverStack.cpp
tests/Reactive2/
├── SignalTests.cpp       (lifted from prototype)
├── ComputedTests.cpp
├── EffectTests.cpp
├── ScopeTests.cpp
├── DiamondTests.cpp
└── BindableTests.cpp     ~100 lines (round-trip T <-> closure)
```

**Integration with existing types:**
- `Signal<T>` in `Reactive2` uses the same `Color`, `Point`, `Size`, `Rect` from `Flux/Core/Types.hpp`.
- The existing `Animation<T>` in `Flux/Reactive` is *not* touched in this stage. It will be ported in Stage 5 alongside the hooks rewrite.

**Gate:**
- `tests/Reactive2/` is fully green under ASAN/UBSAN/TSAN.
- v4 tests still pass (the old `Flux/Reactive` is untouched).
- Bindable round-trip tests confirm the value-or-closure detection works for `int`, `float`, `Color`, `std::string`, `EdgeInsets`.

**Output artifact:** `tests/Reactive2/` test binary, all green.

## 5. Stage 3 — Element + Bindable refactor

**Goal:** Strip body-rerun machinery out of `Element` and replace property setters with `Bindable<T>`-accepting overloads.

**This is the most invasive stage.** Almost every `.cpp` file in `src/UI/` touches `Element` somewhere, and `Element`'s API is what every example uses. Plan: do the surgery in a single multi-day session, fix all compilation errors as a single pass, and don't try to keep the framework runnable mid-stage. End-of-stage v5 has Element correct but no Mount path yet — gate is "Element tests pass and Element-using code compiles," not "anything renders."

**Subtasks:**

**3a. Slim Element's Concept interface.**
File: `include/Flux/UI/Element.hpp`, `src/UI/Element/ElementTypeErasure.cpp`.

Remove these virtual methods:
- `expandsBody()`
- `resolveCompositeBody(...)`
- `buildMeasured(...)`
- `valueEquals()`, `structuralEquals()`

Replace with:
- `mount(MountContext& ctx, std::unique_ptr<SceneNode>* outNode)` — replaces `buildMeasured`. Materializes a scene node, installs reactive bindings, returns. Called once per Element-instance per mount.

`Element::measure(...)` survives, but its job changes: it's called by the layout pass on retained scene nodes, not by the (now-deleted) body-rerun pass.

`measureId_` is deleted. Element no longer has identity across rebuilds because there are no rebuilds.

**3b. Add Bindable<T> overloads to Element modifiers.**
File: `include/Flux/UI/Element.hpp`, `src/UI/Element/ElementModifiers.cpp`.

Each modifier currently of the form `Element foo(T value) &&` gains a sibling overload `Element foo(Bindable<T> value) &&`. Internally the value-`T` overload constructs a `Bindable<T>` and calls through, so there's a single implementation path.

Modifiers updated this way:
- `size(float, float)`, `width(float)`, `height(float)`
- `fill(FillStyle)`, `fill(Color)`
- `stroke(StrokeStyle)`, `stroke(Color, float)`
- `cornerRadius(CornerRadius)`, `cornerRadius(float)`
- `opacity(float)`
- `position(Vec2)`, `position(float, float)`
- `translate(Vec2)`, `translate(float, float)`
- `padding(float)`, `padding(EdgeInsets)`, `padding(float, float, float, float)`
- `shadow(ShadowStyle)`

Modifiers that stay value-only for v5.0:
- `flex(...)`, `key(string)`, `clipContent(bool)`, `focusable(bool)`, `cursor(Cursor)`
- All event handlers (`onTap`, `onPointerDown`, etc.) — these take callbacks anyway.
- `environment<T>(value)`, `overlay(...)`

`ElementModifiers` (the storage struct) gets variant fields for the bindable cases. When a modifier resolves at mount time, it inspects the variant: if it's a value, write directly; if it's a closure, install an Effect that writes whenever the closure's tracked sources fire.

**3c. Delete the body-rerun infrastructure.**
Files to delete entirely:
- `include/Flux/UI/StateStore.hpp` and `src/UI/StateStore.cpp` (~940 lines)
- `include/Flux/UI/BuildOrchestrator.hpp` and `src/UI/BuildOrchestrator.cpp` (~330 lines)
- `include/Flux/UI/Detail/MeasuredBuild.hpp`
- `src/UI/Build/BuildPass.cpp`, `src/UI/Build/ComponentBuildContext.cpp`

Files to slim:
- `include/Flux/UI/Component.hpp`: remove `MeasuredComponent`/`ExpandsBodyComponent` concepts. Keep `BodyComponent`. (The concept body is just `requires(T const& t) { { t.body() }; }` — leave it.)
- `include/Flux/UI/Hooks.hpp`: gut everything. Stage 5 rewrites it; for now, leave a stub header that just `#error`s with "v5 hooks pending — this file is rebuilt in Stage 5". This forces every example/test to fail compilation cleanly until Stage 5, which is intentional — we want the compiler to enumerate every callsite.
- `include/Flux/UI/SceneBuilder.hpp` and `src/UI/SceneBuilder.cpp`: delete the body-rerun-driven build path. The mount-time path moves into a new `MountContext` (Stage 4). SceneBuilder itself is deleted; its name doesn't survive.
- `include/Flux/Detail/Runtime.hpp` and `src/UI/Runtime.cpp`: keep the class but rip out `BuildOrchestrator` and the rebuild-scheduling code. Stage 4 fills it back in with the mount-root logic.

**3d. Fix the compile errors in v4 tests.**
A handful of v4 unit tests directly use `StateStore`, `SceneBuilder`, etc. Mark these as `[v5-stage5]` for now: comment out their CMake registration with a TODO. They'll be ported in the appropriate later stage. Do not delete them — they're the regression suite.

Tests that survive Stage 3:
- `tests/SceneGraphTests.cpp` (scene graph is preserved)
- `tests/AnimationTests.cpp` (Animation type isn't touched until Stage 5; but its Hooks.hpp dependency means this compiles only after Stage 5)
- `tests/ParagraphCacheTests.cpp`, `tests/ParagraphCacheIncrementalTests.cpp` (unrelated to the reactive core)
- `tests/SemanticThemeTests.cpp` (theme struct unchanged)
- `tests/SceneTraversalTests.cpp`, `tests/SceneGeometryIndexTests.cpp` (scene-graph internals)
- `tests/MetalCanvasTests.mm` (graphics)

Tests parked until Stage 5 or 6:
- `tests/AlertTests.cpp` — pulls views
- `tests/AnimationTests.cpp` — pulls hooks
- `tests/RuntimeTests.cpp` — pulls runtime
- `tests/SceneBuilderLayoutTests.cpp`, `tests/SceneBuilderReuseTests.cpp` — replaced by mount tests in Stage 4
- `tests/LayoutAlgorithmTests.cpp` — pulls runtime context
- `tests/TextEditUtilsTests.cpp` — survives if it's pure-utility; check at refactor time

**Gate:**
- `cmake --build` succeeds. The library compiles. Examples will not link (Hooks.hpp is `#error`); this is fine. Library targets must be green.
- Surviving tests pass.
- `git grep -E "StateStore|BuildOrchestrator|MeasuredBuild|markCompositeDirty|buildMeasured|expandsBody|resolveCompositeBody|reactiveDirty|markReactiveDirty"` returns zero hits in `src/`, `include/`, and surviving tests.

**Output artifact:** Library compiles; surviving tests green.

## 6. Stage 4 — Mount root + Scope tree

**Goal:** Wire up the mount path. After this stage, `hello-world` renders.

**4a. MountContext + MountRoot.**
New files:
```
include/Flux/UI/MountContext.hpp     ~80 lines
include/Flux/UI/MountRoot.hpp        ~60 lines
src/UI/MountContext.cpp              ~150 lines
src/UI/MountRoot.cpp                 ~150 lines
```

`MountContext` carries:
- Current `Owner*` (Scope handle)
- Current `EnvironmentStack` reference
- Pointer to the `TextSystem`
- Pointer to the `MeasureContext`
- Pointer to the parent `SceneNode` being assembled

`MountRoot` is what `Runtime` owns. It takes a root component value, creates a root `Scope`, calls `body()` on the component **once**, walks the resulting `Element` tree, calls `Element::mount(ctx, ...)` recursively, attaches the resulting scene-node tree to the window. Returns when mounting is done.

Re-entry into MountRoot only happens on root-component swap (which is a rare developer-initiated thing, not a per-frame thing). At that point: dispose the root Scope, detach scene nodes, rebuild from scratch. **There is no incremental remount path.** Per-frame updates are entirely scene-node mutations driven by Effects.

**4b. Element::mount implementation.**
File: `src/UI/Element/ElementTypeErasure.cpp`.

For each Element type, `mount()`:
1. Call `MeasureContext` to size the element if it's a primitive (Rectangle, Text, Image, etc.).
2. Materialize the corresponding `SceneNode` subclass.
3. For each `Bindable<T>` modifier, inspect the variant:
   - If value: write directly to the scene node (e.g., `node->setSize(value)`).
   - If closure: create an `Effect` owned by the current Scope that re-evaluates the closure and writes to the scene node. The effect captures the scene-node pointer; safe because the scene node's lifetime is bounded by the Scope (cleanup at the end of the Scope teardown nulls the captured pointer via a `Cleanup` registration).
4. For composite components (those with `body()`), call `body()` once, then recursively `mount()` the returned Element under a child Scope.
5. For container elements (VStack/HStack/ZStack/Grid), iterate `children`, recursively mount each, append to the current scene node.
6. Return the materialized scene node.

**Reactive control flow placeholder.** `For` and `Show` are not in this stage — they come in Stage 6. For now, treat any `ForEach`/conditional-children patterns in tests/examples as compile errors. (Stage 4's gate is hello-world specifically, which has no dynamic lists.)

**4c. Wire Runtime to MountRoot.**
File: `src/UI/Runtime.cpp`.

`Runtime::setRoot()` constructs a `MountRoot`, mounts it. Per-frame work shrinks dramatically: layout pass walks the scene graph (unchanged), render pass walks the scene graph (unchanged). The "rebuild" scheduling code is gone. The frame loop becomes pure drain-effects → layout → render.

`Effects` schedule themselves to run on the next frame's pre-layout phase. The scheduling primitive is a per-window `EffectQueue` (just a vector of pending effect pointers, drained at frame start). When `Signal::set()` propagates to an effect, the effect adds itself to the queue. Same idea v4 had with its `signalBridgeMarkReactiveDirty`, but per-effect instead of "rebuild the whole tree."

**4d. Resurrect minimum testing.**
- New test file `tests/MountRootTests.cpp` — exercises mounting a trivial component, checks the resulting scene-node tree shape, checks Scope teardown disposes effects and nulls scene-node references.
- `tests/SceneBuilderLayoutTests.cpp` and `tests/SceneBuilderReuseTests.cpp` — these tested the v4 incremental-reuse behavior. The reuse problem doesn't exist in v5 (scene nodes are mounted once and persist). Port the still-relevant assertions (correct geometry, correct child ordering) into the new `MountRootTests`. Delete the rest.

**4e. Port hello-world.**
File: `examples/hello-world/main.cpp`.

```cpp
struct HelloRoot {
  auto body() const {
    auto theme = useEnvironment<Theme>();
    return Text{
      .text = "Hello, World!",
      .font = Font::largeTitle(),
      .color = Color::primary(),
      ...
    };
  }
};
```

The body is unchanged — it's a static component. The point of porting it is to hit the `Text{}` aggregate-init path with `Bindable<std::string>` (constructed from `std::string` literal), validate that mount creates the scene node, layout sizes it correctly, render draws it. **`useEnvironment<Theme>()` returning `Signal<Theme>` doesn't compile yet** (Stage 5 owns that), so for this stage either stub it as returning `Theme const&` directly, or temporarily inline `Theme::light()` — and document the stub.

**Gate:**
- `tests/MountRootTests.cpp` passes.
- `examples/hello-world` builds, launches, draws "Hello, World!" centered on a window.
- `Runtime::setRoot` followed by destroying the runtime shows no leaks (ASAN clean, RSS stable).
- `git grep "BuildOrchestrator\|StateStore"` still returns zero hits.

**Output artifact:** hello-world running.

## 7. Stage 5 — Hooks rewrite

**Goal:** Restore `useState`, `useEffect`, `useComputed`, `useAnimation`, `useEnvironment`, `useFocus`, `useHover`, `usePress`, `useViewAction`, `useWindowAction`. Once-at-mount semantics. Examples that use these start working.

**5a. Rewrite Hooks.hpp.**
File: `include/Flux/UI/Hooks.hpp`. Delete the `#error` stub from Stage 3.

```cpp
namespace flux {

template<typename T>
Signal<T> useState(T initial = T{}) {
    Owner* owner = Owner::current();
    assert(owner && "useState called outside body()");
    return owner->makeOwnedSignal<T>(std::move(initial));
}

template<typename Fn>
auto useComputed(Fn&& fn) {
    Owner* owner = Owner::current();
    assert(owner && "useComputed called outside body()");
    return owner->makeOwnedComputed<std::invoke_result_t<Fn&&>>(std::forward<Fn>(fn));
}

void useEffect(SmallFn<void()> fn);

template<Interpolatable T>
Animation<T> useAnimation(T initial = T{});  // see 5b

template<typename T>
Signal<T> useEnvironment();  // see 5c

Signal<bool> useFocus();
Signal<bool> useHover();
Signal<bool> usePress();
// etc.
}
```

The signal/computed/effect handles are *value types* that hold a `shared_ptr` to the underlying node. Capturing them in lambdas is cheap and safe.

**5b. Animation integration.**
File: `include/Flux/Reactive2/Animation.hpp`, `src/Reactive2/Animation.cpp` (new — port from old `Flux/Reactive/Animation.hpp`).

`Animation<T>` is restructured to wrap a `Signal<T>` internally. Reads through the underlying signal subscribe normally. The animation tick advances the value via `signal_.set()`, which propagates through the reactive graph as any other write would.

`useAnimation<T>(initial, options)` constructs an `Animation<T>` owned by the current Scope. Returns a value-typed handle (same shape as v4's `AnimationHandle<T>` — `*animation` reads, `animation = value` writes, `.play()`/`.pause()` work as before).

The `WithTransition` thread-local scope mechanism is preserved unchanged.

`scheduleReactiveRebuildAfterAnimationChange` is deleted — animations writing to their signal triggers effects directly via the reactive graph.

**5c. Environment + theme.**
File: `include/Flux/UI/Environment.hpp`.

The environment stack stays — it's the right shape. What changes: each environment slot stores a `Signal<T>` (or any reactive-shaped type) instead of a value. `useEnvironment<T>()` walks the stack and returns the signal handle. Lookup result is cached at mount time on the requesting Scope, so the walk happens once.

For Theme specifically, the window pushes a `Signal<Theme>` onto the environment stack at startup. Components reading `useEnvironment<Theme>()` get that signal. Reading `theme().space3` inside `body()` does *not* subscribe (because body() runs once and is not a tracking context). Reading `theme().space3` inside a leaf-level closure (Bindable<float>) subscribes only to the theme signal — recomputes the closure when theme changes.

For granularity: ship a `themeField(&Theme::space3)` helper in `include/Flux/UI/Theme.hpp` that returns a `Computed<float>`. Components that want to subscribe to a single field use this. Default is whole-theme.

**5d. Focus/hover/press/actions.**
Files: `src/UI/FocusController.cpp`, `src/UI/HoverController.cpp`, `src/UI/GestureTracker.cpp`, `src/UI/ActionRegistry.cpp`.

These already track per-key state. The change: instead of marking composite dirty when state changes, write into the per-component `Signal<bool>` exposed by `useFocus`/`useHover`/`usePress`. The signal is Scope-owned, so it's torn down with the component.

`useFocus()` etc. allocate a Signal at mount time, register interest with the appropriate controller (keyed by some component identity — Stage 6 will give us scoped IDs from the Scope tree; for Stage 5 use a simple incrementing scope-id counter). When the controller observes a state change, it writes to the signal.

`useViewAction` and `useWindowAction` similarly: register a handler keyed by the current Scope; cleanup-on-Scope-disposal automatically deregisters.

**5e. Restore tests.**
- `tests/AnimationTests.cpp` — port to v5 hooks. Should largely be unchanged; `useAnimation` API is preserved.
- `tests/AlertTests.cpp` — view-level test; should work once views are mountable.
- `tests/RuntimeTests.cpp` — port to v5 mount path.

**5f. Port mid-complexity examples.**
- `examples/reactive-demo` — already uses `Signal`/`Computed` directly at window scope (not via hooks). Verify it works in v5; it should be the simplest port because it bypasses hooks entirely.
- `examples/animation-demo` — AmbientLoopLab. The canonical test. The `body()` runs once. The animation tick advances `phase` signal. Each bar's `Rectangle{}.size(22.f, [=]{ return 18.f + emphasis * 34.f; })` reactively updates only the bar's height. **The CPU number measured here at end of Stage 5 is the headline v5 perf result.**
- `examples/theme-demo` — uses ~25 `useState` calls in one component. Validates Scope ownership of many signals.

**Gate:**
- `tests/AnimationTests.cpp`, `tests/AlertTests.cpp`, `tests/RuntimeTests.cpp` pass.
- `examples/reactive-demo`, `examples/animation-demo`, `examples/theme-demo` all build, launch, render correctly.
- AmbientLoopLab runs at 60 fps with **measured CPU under 5%** on the same M-series Mac the v4 baseline was measured on. Recorded in `optimization-attempts.md` as the v5 baseline.
- ASAN/UBSAN clean; no leaks across mount/unmount cycles.

**Output artifact:** AmbientLoopLab number recorded; three examples running.

## 8. Stage 6 — Reactive control flow

**Goal:** `For`, `Show`, `Switch` reactive primitives, with proper mount/unmount lifecycle and child Scope ownership.

**6a. For<T>.**
File: `include/Flux/UI/Views/For.hpp`, `src/UI/Views/For.cpp`.

Replaces v4's `ForEach<T>`. API:

```cpp
template<typename T, typename KeyFn, typename Factory>
auto For(Signal<std::vector<T>> items, KeyFn keyFn, Factory factory);
```

`items` is reactive — when it changes, `For` reconciles. `keyFn` is mandatory: `Key(T const&) -> K` where `K` is hashable and equality-comparable. `factory` is `Element(T const&, Signal<size_t> index)` — receives the item (immutable copy) and a signal carrying the current index, useful for animations.

Reconciliation algorithm (Solid's approach):
1. Build `unordered_map<K, ChildState>` from the previous run.
2. Walk new items in order. For each item:
   - Look up by key. If found in old map: reuse (move scene node + child Scope to new position; remove from old map).
   - If not found: create new child Scope, mount factory output, append.
3. Remaining entries in old map are unmounted: dispose Scope (which cleans up effects, scene nodes, subscriptions).

Each row owns a child `Scope`. The row's `body()` does *not* re-run when item value mutates — mutations flow through item-internal signals (which the user sets up explicitly in factory).

**6b. Show.**
File: `include/Flux/UI/Views/Show.hpp`, `src/UI/Views/Show.cpp`.

```cpp
auto Show(SmallFn<bool()> pred, SmallFn<Element()> thenFn, SmallFn<Element()> elseFn = {});
```

Owns a single child Scope. Predicate is reactive. When it flips, dispose child Scope + scene node, mount the new branch.

**6c. Switch.**
Same shape as Show but selects among N branches via a comparator. Implementation is straightforward generalization of Show.

**6d. Tests.**
- `tests/ForTests.cpp` — insert/remove/reorder/replace; verify row-body-runs-once invariant; verify dispose-on-unmount cleans up scope-owned signals.
- `tests/ShowTests.cpp` — predicate flips; child Scope teardown; effects in subtree dispose.

**6e. Port list-using examples.**
- `examples/scroll-demo` — uses dynamic lists. Re-author with `For`. Note: this example previously built `vector<Element>` manually with for-loops in a static `body()`. In v5, those static lists can stay as plain `vector<Element>` (mount-time-only generation). Use `For` only where the list is reactive (changes after mount).
- `examples/lambda-studio` — large; uses one big `useState<AppState>`. Should mostly Just Work once useState is in. Dynamic lists in the chat/conversation UI become `For`.
- `examples/listview-demo` if there's reactive list content.

**Gate:**
- `tests/ForTests.cpp`, `tests/ShowTests.cpp` pass.
- `examples/scroll-demo`, `examples/lambda-studio` build and run.
- Reordering a `For` list of 100 rows preserves per-row Scope state (a `useState<int>` inside each row keeps its value when its row moves).
- Replacing a `For` list (entirely new keys) disposes all old child Scopes (verifiable via a per-Scope dispose counter in debug builds).

**Output artifact:** scroll-demo and lambda-studio running.

## 8.5. Stage 7 — Theme + environment reactivity

**Goal:** Theme switching at runtime is fully reactive. No remount, no flicker, no dropped frames.

**7a. Window pushes a Signal<Theme> at startup.**
File: `src/Core/Window.cpp`.

The window owns a `Signal<Theme> themeSignal_` initialized to `Theme::light()` (or whatever the platform default is). The environment stack pushes this signal at mount time.

`Window::setTheme(Theme)` writes to the signal. Effects across the tree fire and update scene-node properties.

**7b. Verify granular subscription works.**
A component reading `useEnvironment<Theme>()` and using `theme().space3` inside a `Bindable<float>` closure subscribes to the whole theme signal. That's correct behavior — coarse granularity, but theme changes are rare so this is fine.

A component using `themeField(&Theme::space3)` instead gets a `Computed<float>` that recomputes only when `space3` changes between two themes. Useful for the rare case (a custom-theme editor, etc.).

**7c. Test.**
- `tests/ThemeReactivityTests.cpp` (new) — toggle theme, verify all theme-dependent leaves update without a remount, verify Scope tree is unchanged across the toggle.

**7d. Port theme-demo if not done in Stage 5.**
The theme-demo's dark-mode toggle is the integration test for this stage. Toggling dark mode should:
- Cause every theme-dependent property to update.
- Not dispose any scopes.
- Not remount any components.
- Frame budget stays under 16.67 ms during the toggle (single frame).

**Gate:**
- Theme reactivity test passes.
- theme-demo's dark-mode toggle is a single-frame, no-remount transition.
- Scope tree before/after toggle is structurally identical (assertable via a debug-build scope dumper).

**Output artifact:** theme-demo dark/light toggle working without flicker.

## 9. Stage 8 — Examples migration

**Goal:** All 31 examples build and run. Each ported example is a regression test for its category.

**Strategy:** port in dependency order. Each port is a single PR-sized change, even though we're not doing PRs internally. Each one is a checkpoint.

**Port order (rough — adjust as we discover dependencies):**

1. Examples already ported through earlier stages (sanity check they still work):
   hello-world, reactive-demo, animation-demo, theme-demo, scroll-demo, lambda-studio.

2. Static-content examples (no reactive state — easiest):
   scene-graph-demo, blend-demo, layout-demo, typography-demo, text-demo, cursor-demo.

3. Single-state examples (one or two `useState`s):
   button-demo, checkbox-demo, toggle-demo, slider-demo, segmented-demo, select-demo, alert-demo, popover-demo, tooltip-demo, badge-demo (if it has state).

4. List/table examples (use `For`):
   table-demo, listview-demo (if exists).

5. Complex/composed examples:
   card-demo, image-demo, icon-demo, markdown-formatter-demo, textinput-demo, toast-demo.

For each example:
- Update `body()` to v5 conventions (build-once mental model, hooks return Signals).
- Replace any `ForEach` usage with `For`.
- Replace any conditional-rendering pattern (which v4 didn't formalize but examples likely fake with `if`-based child filtering) with `Show`/`Switch`.
- Verify it renders and behaves identically to v4. Take screenshots if there's any doubt.
- Run for 60 seconds in a benchmark mode (if applicable) and check ASAN/RSS.

**Gate:**
- All 31 examples build via `cmake --build`.
- All 31 examples launch and pass a manual visual smoke test.
- Per-example CPU at idle: < 7% on M-series (v4 was 10% post-optimization; v5 should beat it).
- Per-example CPU under load (animations, scrolling): record numbers, compare to v4.
- ASAN clean across all examples.

**Output artifact:** Updated examples directory; per-example perf numbers added to `optimization-attempts.md`.

## 10. Stage 9 — Cleanup, perf validation, release

**Goal:** v5 is the new main. Codebase is clean, perf is validated, docs are accurate.

**9a. Rename `Reactive2` → `Reactive`.**
The old `include/Flux/Reactive/` and `src/Reactive/` were emptied during Stage 3 (only `Animation.hpp`, `Transition.hpp`, etc. remained, and even those got pulled into `Reactive2` during Stage 5). At this point the old `Reactive` directory should be empty or near-empty.

```sh
rm -rf include/Flux/Reactive src/Reactive
git mv include/Flux/Reactive2 include/Flux/Reactive
git mv src/Reactive2 src/Reactive
git mv tests/Reactive2 tests/Reactive
# Find/replace all `#include <Flux/Reactive2/...>` to `#include <Flux/Reactive/...>`
# Find/replace all `flux::Reactive2::` to `flux::Reactive::` (probably unused, but check).
```

Run all tests + examples after the rename. They should be unchanged.

**9b. Delete dead code.**
Tools:
```sh
git grep "TODO.*v5"      # find every unresolved v5-stage marker
git grep -E "StateStore|BuildOrchestrator|MeasuredBuild|markCompositeDirty"  # should still be zero
git grep "useMemo"       # should be zero
git grep "useOuterElementModifiers"  # should be zero
```

Look for unused `Element::Concept` virtual methods, unused fields in `StateSlot` (whole struct should be gone), unused enums.

Compiler warnings on `-Wunused-*` are addressed.

**9c. Documentation pass.**
Update:
- `README.md` — top-level architecture section (replace body-rerun description with build-once + fine-grained).
- `docs/conventions.md` — adjust style examples for new hook return types and Bindable<T> patterns.
- `docs/composites.md` — rewrite. The composite contract changed (body() runs once).
- `docs/event_queue.md` — review for obsolete references.
- `docs/ui-view-body-style.md` — rewrite around build-once.
- New: `docs/migrating-to-v5.md` — short, even though we're hard-cutover; covers the mental-model shift.
- New: `docs/reactive-graph.md` — internals doc explaining Signal/Computed/Effect/Scope mechanics, the three-state coloring, the SBO callable, the diamond-freedom story. Audience is future contributors; about 1500 lines.
- New: `docs/v5-final-perf.md` — tabular comparison: v4-baseline (36% CPU), v4-final (10%), v5 (target ≤ 5%). Per-example numbers from Stage 8.

**9d. Final perf validation.**
- AmbientLoopLab steady-state CPU: target ≤ 5%, hard ceiling 7%.
- AmbientLoopLab steady-state allocations per frame: target 0 in the reactive layer (effects don't allocate during steady-state fires).
- Memory: 24-hour soak test of the animation-demo binary. RSS growth ≤ 100 KB total.
- Cold mount of lambda-studio: target < 50 ms from `setRoot()` to first frame.
- Hot-path: reordering a 1000-row `For` list with all keys preserved: target < 5 ms.
- Hot-path: replacing a 1000-row `For` list with all-new keys: target < 30 ms.

If any of these miss by > 2×, perf-fix before promoting to main.

**9e. CI hardening.**
- Add a benchmark CI job that runs the AmbientLoopLab benchmark on every commit and posts the number.
- Add a leak-check CI job (ASAN running every example for 10 seconds).
- Add a graph-shape regression test: dump the reactive-graph topology of animation-demo at t=0, store the baseline, fail CI if the topology changes accidentally.

**9f. Promotion.**
```sh
git tag v4-final main
git checkout main
git reset --hard v5
git push --force-with-lease origin main
git push origin v4-final
```

Update GitHub default-branch protections, archive the `v5` branch, write a brief release note.

**Gate:**
- All examples build and run.
- All tests pass under ASAN/UBSAN/TSAN.
- Perf numbers are recorded in `docs/v5-final-perf.md` and meet the targets.
- Documentation is current (no `Reactive2`, no `StateStore`, no `useMemo` references anywhere).

**Output artifact:** v5 is `main`.

## 11. Risk register

Real risks, with concrete mitigations:

**R1 — Bindable<T>'s closure path is too slow at scale.**
*Trigger:* Stage 1 benchmark misses the < 500 ns/effect target.
*Likelihood:* medium. SBO callables in C++ are subtle to get right.
*Mitigation:* the prototype gates this. If we miss in Stage 1, options are (a) revisit SBO size (32 → 48 → 64 bytes), (b) consider templating Effects on the closure type for hot leaves (CRTP fast path), (c) fall back to `std::function` and accept the perf loss but ship.

**R2 — Owner tree teardown order has bugs that only show at scale.**
*Trigger:* Stage 5–6 example soak tests show leaks or use-after-free.
*Likelihood:* medium. Cleanup ordering is the #1 source of bugs across every fine-grained framework's history.
*Mitigation:* aggressive ASAN/TSAN coverage from Stage 1; explicit `Disposed` flag on every node with a debug-build assertion on access; generation counters on handles to detect ABA.

**R3 — Animation integration is awkward.**
*Trigger:* Stage 5 finds that `Animation<T>` wrapping a `Signal<T>` has the wrong shape — e.g., `WithTransition` doesn't compose with reactive reads cleanly.
*Likelihood:* medium-low. v4's Animation is already structurally close to what v5 needs.
*Mitigation:* prototype the AmbientLoopLab port in Stage 1 with a stub Animation; if the shape is wrong we find out before committing.

**R4 — `For` reconciliation is buggy on edge cases (duplicate keys, empty lists, single-item lists).**
*Trigger:* Stage 6 tests find issues; Stage 8 examples uncover more.
*Likelihood:* medium. Solid's For has had multiple bug fixes over the years.
*Mitigation:* port Solid's test suite for `<For>` directly (translated to C++) in Stage 6. It's exhaustive; we get the bug catalog for free.

**R5 — Theme reactivity at the whole-theme granularity causes too many effects to fire on theme switch.**
*Trigger:* Stage 7 measurement shows theme switch takes > 16.67 ms.
*Likelihood:* low for a typical app, medium for theme-demo specifically (it has many components).
*Mitigation:* if we miss the gate, fall back to the per-field theme signal model (Theme::Reactive struct of Signals) for the heavy components.

**R6 — Examples have hidden assumptions about body-rerun semantics.**
*Trigger:* Stage 8 ports uncover code that read state in `body()` expecting it to refresh on the next frame.
*Likelihood:* high. This is the React-mental-model trap.
*Mitigation:* document the "hooks return Signals; reads in body() don't subscribe" rule loudly in `docs/migrating-to-v5.md`. Provide a debug-build warning when a `body()` reads a Signal's value directly via `.peek()` semantics (i.e., reading inside a non-reactive context that previously would have re-run). For ambiguous cases, lift the read into a `Bindable<T>` closure.

**R7 — Lambda-studio (the largest example, 2024 lines) has subtle state-tree assumptions that break.**
*Trigger:* Stage 6 or 8 port reveals brokenness.
*Likelihood:* medium. It uses one huge `useState<AppState>`, which conceptually maps fine, but it likely has spots that depend on `useState` slot-positional stability (which is what v4 does and v5 doesn't).
*Mitigation:* lambda-studio is the integration test for v5. If it breaks badly, that's the signal that the design has a missing piece — fix the design, don't paper over the example.

## 12. Validation discipline

A few practices that keep the staging honest:

**Each gate is binary.** It either passes or it doesn't. "Almost passes" is not pass.

**Numbers are recorded the same day they're measured.** `optimization-attempts.md` already serves as the perf log; v5 numbers go there.

**Per-stage commits are atomic.** A stage is one logical change, even if it's spread across many files. `git log --oneline v4-final..main` after promotion should read like a stage-by-stage progression.

**ASAN is the default test mode.** Not just a CI job — local development too. Catches use-after-free in ownership-graph mistakes, which is the #1 risk.

**Don't skip the prototype.** Stage 1 is doing real work that informs Stage 2. The temptation to "just start writing the real thing" is the single most expensive mistake we could make.

**The build is always green.** A stage isn't done until everything compiles. The Stage 3 hooks-stub `#error` is the exception — that's a deliberate compile gate.

## 13. Out of scope for v5.0 (explicit non-goals)

These are deferred to v5.1+ and tracked separately. Calling them out now to prevent scope creep:

- **Async reactivity.** No Promise/coroutine integration in the reactive graph. Tracking ends at `co_await`.
- **Cross-thread signals.** Reactive graph is single-threaded, period. Document, enforce with thread_local + assertions.
- **A `LegacyView` adapter.** No v4-compat layer. Hard cutover.
- **Compile-time graph generation.** No clang plugin, no preprocessor magic, no codegen step. Standard C++23 toolchain.
- **DevTools UI.** Provide JSON dump endpoint and `inspect_trace()` API; visualization tooling is post-v5.0.
- **Linux Wayland backend parity validation.** v5 must compile and the reactive core must work on Linux, but the per-platform polish pass for Wayland scrolling/animation perf is a separate effort. Cross-platform parity claim in v5.0 is "structurally complete and runs"; "perf-tuned on both" is v5.1.
- **Hot reload.** The reactive graph's structure makes hot reload more achievable than v4's, but actually wiring it to a file-watcher + dynamic reload is post-v5.0.

## 14. Summary

Ten stages, each a gate. Prototype first. Hard cutover, no parallel runtime. Net code reduction despite adding the new reactive runtime, because v4's body-rerun apparatus is larger than its replacement. Headline metric: AmbientLoopLab CPU drops from 10% (v4-final) to ≤ 5% (v5 target). Architecture is locked; only the API specifics flex during the prototype.

The next concrete action is Stage 0 — branch and scaffolding. From there, Stages 1 through 9 in order. No skipping. The plan is the contract.
