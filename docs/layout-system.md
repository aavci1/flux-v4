# Flux Layout System

## Architecture Overview

Flux uses a **two-phase, type-erased layout** model. There is no base `View` class — views are plain structs that satisfy one of three C++ concepts. A universal wrapper called `Element` type-erases them and dispatches `layout`, `measure`, and `renderFromLayout` through a virtual `Concept`/`Model<C>` pattern.

The pipeline for each rebuild has three distinct phases:

```
Phase 1: Layout   — Element tree → LayoutTree
Phase 2: Render   — LayoutTree + EventMap + SceneGraph
Phase 3: Paint    — SceneGraph → Canvas (unchanged each frame; pure renderer)
```

**Phase 1** walks the element tree, runs `measure` and flex distribution, and writes a `LayoutTree` of `LayoutNode`s — geometry and structure only, no SceneGraph, no EventMap. The only dependencies are `LayoutConstraints`, `LayoutHints`, and `TextSystem` (for text measurement).

**Phase 2** walks the `LayoutTree` and emits SceneGraph nodes (`RectNode`, `TextNode`, `LayerNode`, etc.) and `EventMap` entries via `renderLayoutTree`. Each `LayoutNode` carries the resolved `frame`, so the render phase only creates the appropriate node at the computed position.

**Phase 3** is `SceneRenderer::render`, which walks the SceneGraph and draws to Canvas. Already fully separated; unchanged by the layout/render split.

### Three Kinds of Components

Defined in `include/Flux/UI/Component.hpp`:

```cpp
// A composite has a body() that returns its subtree.
template<typename T>
concept CompositeComponent = requires(T const& t) { { t.body() }; };

// A render leaf draws directly and reports its own size.
template<typename T>
concept RenderComponent = requires(T const& t, Canvas& c, Rect r, LayoutConstraints const& cs,
                                 LayoutHints const& h) {
  { t.render(c, r) };
  { t.measure(cs, h) } -> std::convertible_to<Size>;
} && !CompositeComponent<T>;

// A primitive handles layout and render as separate methods.
template<typename T>
concept PrimitiveComponent =
    requires(T const& t, LayoutContext& lctx, RenderContext& rctx, LayoutNode const& node,
             LayoutConstraints const& c, LayoutHints const& h, TextSystem& ts) {
      { t.layout(lctx) };
      { t.measure(lctx, c, h, ts) } -> std::convertible_to<Size>;
      { t.renderFromLayout(rctx, node) };
    } && !CompositeComponent<T> && !RenderComponent<T>;
```

**Composite components** (e.g. `Button`, `Slider`, `Toggle`) define `body()` returning an `Element`. They never emit scene nodes directly.

**Render components** (e.g. custom drawing primitives) implement `render(Canvas&, Rect)` and `measure(LayoutConstraints const&, LayoutHints const&) -> Size`. The framework handles layout node creation and scene emission generically.

**Primitive components** (e.g. `Rectangle`, `Text`, `Image`, `VStack`, `HStack`, `ZStack`) implement all three methods explicitly: `layout(LayoutContext&)` to record a `LayoutNode`, `measure(...)` to report size, and `renderFromLayout(RenderContext&, LayoutNode const&)` to emit scene nodes.

### Child lists (`children()`)

Brace-initialized `.children = { a, b, c }` goes through `std::initializer_list<Element>`, which **copies** each entry into the vector. For hot paths or large trees, use **`children(a, b, c)`** in `include/Flux/UI/Element.hpp` — a variadic helper that `reserve`s and `emplace_back`s each argument without the `initializer_list` copy round-trip.

---

## Core Types

### Geometry (`include/Flux/Core/Types.hpp`)

| Type | Fields | Purpose |
|------|--------|---------|
| `Point` / `Vec2` | `float x, y` | 2D position or offset |
| `Size` | `float width, height` | 2D dimensions |
| `Rect` | `float x, y, width, height` | Axis-aligned bounds |
| `Mat3` | `float m[9]` | 3×3 affine transform (column-major) |
| `CornerRadius` | `float topLeft, topRight, bottomRight, bottomLeft` | Per-corner radii |

### Layout Constraints (`include/Flux/UI/LayoutEngine.hpp`)

```cpp
struct LayoutConstraints {
  float maxWidth  = infinity;
  float maxHeight = infinity;
  float minWidth  = 0.f;
  float minHeight = 0.f;
};

struct LayoutHints {
  std::optional<VerticalAlignment>   hStackCrossAlign;   // set by HStack per child
  std::optional<HorizontalAlignment> vStackCrossAlign;  // set by VStack / ForEach per child
};
```

Numeric **constraints** flow top-down (`LayoutContext::constraints()`). **Hints** are a parallel stack (`LayoutContext::hints()`) for cross-axis alignment — not size bounds. Parents pass both when calling `child.measure(ctx, childCs, childHints, textSystem)` and `scope.layoutChild(child, frame, cs, hints)`.

### LayoutEngine (`include/Flux/UI/LayoutEngine.hpp`)

A minimal holder for the **current child frame** during the layout pass:

```cpp
class LayoutEngine {
public:
  void resetForBuild();
  void setChildFrame(Rect frame);
  Rect consumeAssignedFrame();   // debug: asserts parent set frame for this child
  Rect lastAssignedFrame() const;
};
```

Parent containers call `setChildFrame(...)` before laying out each child. Layout containers and leaves call `consumeAssignedFrame()` to read that assignment.

### LayoutTree (`include/Flux/UI/LayoutTree.hpp`)

The key intermediate data structure. Produced by Phase 1, consumed by Phase 2.

```cpp
struct LayoutNode {
  LayoutNodeId id{};
  LayoutNodeId parent{};
  Rect frame{};           // bounds in the current layer's local coordinate system
  Rect worldBounds{};     // axis-aligned bounds in window / root space, computed from
                          // the *parent's* layer world transform at the time of pushLayoutNode
  Kind kind = Kind::Leaf; // Container, Leaf, Modifier, Composite
  std::vector<LayoutNodeId> children;
  LayoutConstraints constraints{};
  LayoutHints hints{};
  ComponentKey componentKey{};
  ElementModifiers const* modifiers = nullptr; // for Modifier nodes
  Element const* element = nullptr;            // source element for the render phase
  // ... additional render-phase metadata (ContainerLayerSpec, etc.)
};

class LayoutTree {
public:
  LayoutNodeId root() const;
  LayoutNode const* get(LayoutNodeId id) const;
  std::span<LayoutNode const> nodes() const;      // depth-first order
  std::optional<Rect> rectForKey(ComponentKey const& key) const;
  Rect unionSubtreeWorldBounds(LayoutNodeId nodeId) const;
};
```

The `LayoutTree` holds **geometry only**: frame rects, parent-child relationships, component keys, and modifier metadata. Fill colors, stroke styles, text content, images — these live on the original `Element` / component structs and are read during Phase 2.

### Layout stderr tree (`FLUX_DEBUG_LAYOUT`)

When the environment variable `FLUX_DEBUG_LAYOUT` is set to a non-empty value other than `0`/`false`, each rebuild prints a text tree to **stderr**: one line per container and leaf as the layout walk visits nodes. Lines include **constraints** (`maxWidth`×`maxHeight`), **measured** size, **frame** (`Rect`), and optional **flex** on leaves.

**Visual overlay:** when `Runtime::layoutOverlayEnabled()` is on (toggle **⌘⇧L** — Meta+Shift+L on macOS), `Window::render` draws semi-transparent wireframes over each scene node's layout bounds.

---

## The Full Pipeline

### 1. Trigger

A rebuild is triggered when reactive state changes. `Application::exec()` runs the main loop:

```
event loop iteration:
  1. Process platform events
  2. Dispatch timers
  3. Dispatch event queue
  4. If reactive state is dirty → call next-frame callbacks (triggers rebuild)
  5. If redraw requested → presentAllWindows() → render
```

### 2. Rebuild (`BuildOrchestrator::rebuild`)

```
BuildOrchestrator::rebuild():
  1. graph.clear()                      // wipe the scene graph
  2. layoutEngine_.resetForBuild()      // clear stale child frame
  3. measureCache_.clear()              // fresh measure memoization
  4. stateStore_.beginRebuild()         // prepare reactive state slots

  // ── Phase 1: Layout ─────────────────────────────────────────────────
  5. Create LayoutTree + LayoutContext (TextSystem, LayoutEngine, LayoutTree, MeasureCache)
  6. Push root constraints (window width × height)
  7. rootHolder_->layoutInto(lctx)      // recursive layout pass → fills LayoutTree
  8. layoutRects_.fill(layoutTree, lctx) // post-layout rect cache for useLayoutRect hooks

  // ── Phase 2: Render ──────────────────────────────────────────────────
  9. Create RenderContext (SceneGraph, EventMap, TextSystem)
 10. renderLayoutTree(layoutTree, rctx) // emit SceneGraph nodes + EventMap entries

  11. stateStore_.endRebuild()
  12. Swap event maps, validate focus
  13. Rebuild overlays (same two-phase pattern)
  14. window_.requestRedraw()            // schedule Phase 3 (paint)
```

### 3. The Layout Pass (Phase 1, depth-first)

The layout pass is a single depth-first traversal via `Element::layout(LayoutContext&)`. For each node:

**Composite component** (`body()` exists):
1. Generate a `ComponentKey` for stable identity
2. Push state scope (`StateStore::pushComponent`)
3. Call `value.body()` to get the child `Element`
4. Pop state scope
5. Recursively `child.layout(ctx)` on the body element

**Render leaf** (has `render` + `measure`):
1. `advanceChildSlot()` — consume a slot in the parent's child sequence
2. Resolve bounds from `childFrame()` + constraints via `resolveLeafLayoutBounds`
3. Push a `LayoutNode::Kind::Leaf` to the `LayoutTree`
4. No SceneGraph or EventMap writes

**Primitive leaf** (Rectangle, Text, Image, etc.):
1. `advanceChildSlot()`
2. Resolve bounds
3. Push a `LayoutNode::Kind::Leaf` with frame, element pointer, constraints
4. No SceneGraph or EventMap writes

**Layout container** (VStack, HStack, ZStack, etc.):
1. `advanceChildSlot()` (unless composite body root)
2. Read `parentFrame` from `layoutEngine().consumeAssignedFrame()`
3. Push a `LayoutNode::Kind::Container` with `ContainerLayerSpec`
4. **Measure pass**: iterate children, call `child.measure(ctx, childCs, childHints, textSystem)` collecting sizes
5. Rewind child key index (`ctx.rewindChildKeyIndex()`)
6. Compute flex distribution (grow/shrink) if the main axis is constrained
7. **Layout pass**: for each child, `setChildFrame(...)`, push constraints, `child.layout(ctx)`, pop constraints

### 4. The Render Pass (Phase 2)

`renderLayoutTree` walks the `LayoutTree` and dispatches by `LayoutNode::Kind`:

- **Container** → emit a `LayerNode` with transform from `ContainerLayerSpec`, recurse children
- **Modifier** → emit background `RectNode`, optional `LayerNode` (for opacity/translate/clip), then recurse children
- **Leaf** / **Composite** → call `node.element->renderFromLayout(ctx, node)` which emits the appropriate scene node (`RectNode`, `TextNode`, `ImageNode`, `CustomRenderNode`) and registers `EventMap` handlers

### 5. Measure

`Element::measure` returns a `Size` without side effects (no scene graph writes, no LayoutTree writes). It uses `MeasureCache` for leaf memoization — keyed by `(elementMeasureId, constraints, hints)`, cleared every rebuild.

### 6. Paint (Phase 3)

After rebuild, `window_.requestRedraw()` schedules a paint. `Application::presentAllWindows()` calls `window->render(canvas)`, which invokes `SceneRenderer::render(SceneGraph, Canvas, clearColor)`.

`SceneRenderer` walks the scene graph depth-first:
- `LayerNode` → push transform/clip, recurse into children, pop
- `RectNode` → draw filled/stroked rounded rect
- `TextNode` → draw laid-out text runs
- `ImageNode` → draw image
- `PathNode` → draw vector path
- `CustomRenderNode` → call the `draw` lambda (for `RenderComponent` leaves)

---

## Flex Layout

Children can declare flex hints:

```cpp
struct MyView {
  float flexGrow  = 0.f;   // share of extra space (0 = don't grow)
  float flexShrink = 0.f;  // shrink factor when overflowing (0 = don't shrink)
  float minSize   = 0.f;   // floor on main-axis size during shrink
};
```

Or override them on the `Element` wrapper:

```cpp
Element{MyView{...}}.flex(/*grow=*/1.f, /*shrink=*/1.f, /*minMain=*/50.f)
```

`Spacer` has implicit `flexGrow = 1.f` and `flexShrink = 0.f`.

### Element modifiers (flat storage)

`Element` can carry an optional **`ElementModifiers`** block: padding, background, border, corner radius, opacity, layout-space `position`, post-layout `translate`, clip, event handlers, and an optional fixed `size` / `width` / `height`, and an optional **overlay** subtree. When present, `Element::layout`/`measure` apply these in one pass instead of nesting extra `VStack`/`ZStack`/`Rectangle` wrappers for each modifier.

- **Layout order** (outside-in): effect **layer** (opacity + translation + optional clip rect) when needed → one merged **`LayoutNode::Modifier`** node → **padding** tightens the frame/constraints for the inner view → `impl_->layout` (and overlay layout when set).
- **Render order**: `renderModifier` emits the background `RectNode` / effect `LayerNode`, then recurses into the inner content's nodes.
- **Measure**: constraints are tightened by padding before delegating to the inner implementation; reported size adds padding back; `size` / `width` / `height` modifiers can override width/height on the outer box.
- **CRTP `ViewModifiers`**: view structs expose the same names as `Element` (`padding`, `background`, `flex`, `environment`, …). Chaining produces a single `Element` with accumulated modifier fields.

---

## How VStack / HStack / ZStack Work

All three follow the same pattern with axis-specific differences. See `ContainerLayoutScope` in `src/UI/Layout/ContainerScope.hpp`.

### VStack

**Measure**: Sum of children heights + spacing. Width = max(child widths), clamped to `maxWidth` when finite.

**Layout**:
1. Compute `innerW` from assigned width
2. Measure all children with `maxHeight = ∞`, `maxWidth = innerW`
3. Rewind key index
4. If height-constrained: flex grow/shrink on the Y axis
5. Place each child: `scope.layoutChild(children[i], Rect{0, y, innerW, allocH[i]}, ...)`, advance `y`

### HStack

The horizontal dual of VStack:

**Measure**: Sum of children widths + spacing. Height = max(child heights).

**Layout**:
1. Measure all children with `maxWidth = ∞`
2. If width-constrained: flex grow/shrink on the X axis
3. Place each child: `scope.layoutChild(children[i], Rect{x, 0, allocW[i], rowInnerH}, ...)`, advance `x`

### ZStack

All children share the same space:

**Measure**: `max(children widths) × max(children heights)`, expanded to at least the proposed size.

**Layout**:
1. Measure all children
2. Compute `innerW/innerH` = max of (proposed, largest child)
3. For each child: compute alignment offset, `scope.layoutChild(children[i], {x, y, alignW, alignH}, ...)`

---

## Modifiers and `ViewModifiers` (all protocols)

All three component styles — **`body()` composites**, **`RenderComponent`** custom draw (`measure` + `render` → `CustomRenderNode`), and **primitives** — support the same **`Element` modifier** surface: `padding`, `background`, `border`, `cornerRadius`, `flex`, pointer/keyboard handlers, and so on.

- **Primitives** (`Rectangle`, `Text`, …): inherit `ViewModifiers<Derived>` or wrap in `Element{…}` and chain modifiers; layout/render read `LayoutContext` / `RenderContext` active modifiers where needed.
- **Composites** (`Button`, `TextInput`, …): inherit `ViewModifiers<Derived>`; outer modifiers wrap the subtree via `layoutWithModifiers`. Inside `body()`, **`useOuterElementModifiers()`** returns the wrapper’s `ElementModifiers` when a modifier pass is active (e.g. to align custom chrome with `.background()` / `.border()` / `.cornerRadius()` on `TextInput`). Multiline `TextInput` has no struct-level flex fields — use chained **`.flex(grow, shrink, minMain)`** on the `Element` (same as other views).
- **`RenderComponent`**: inherit `ViewModifiers<Derived>` so `MyView{}.padding(8)` works without spelling `Element{MyView{}}`. Modifier interaction handlers are merged onto the custom-render leaf’s hit target during render.

## How to Implement a New Component

### Option A: Composite Component (recommended for most UI controls)

A composite component defines `body()` returning an `Element`. The framework's default `Element::Model<C>` template handles it automatically.

**Header** (`include/Flux/UI/Views/MySwitch.hpp`):

```cpp
#pragma once
#include <Flux/UI/Element.hpp>
#include <functional>

namespace flux {

struct MySwitch {
  bool isOn = false;
  std::function<void(bool)> onToggle;

  Element body() const;
};

} // namespace flux
```

**Implementation** (`src/UI/Views/MySwitch.cpp`):

```cpp
#include <Flux/UI/Views/MySwitch.hpp>
#include <Flux/UI/Views/ZStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Hooks.hpp>

namespace flux {

Element MySwitch::body() const {
  return Element{ZStack{
    .children = children(
      Element{Rectangle{}}.size(48.f, 24.f).cornerRadius(CornerRadius{12.f})
                          .background(FillStyle::solid(isOn ? Color{} : Color{}))
                          .onTap([this] { if (onToggle) onToggle(!isOn); }),
      Element{Text{.text = isOn ? "ON" : "OFF"}},
    ),
  }};
}

} // namespace flux
```

### Option B: Render Leaf Component

For a custom drawing primitive that doesn't compose existing views.

**Header** (`include/Flux/UI/Views/CircleView.hpp`):

```cpp
#pragma once
#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/Graphics/Canvas.hpp>

namespace flux {

struct CircleView {
  float radius = 20.f;
  Color fill{};

  void render(Canvas& canvas, Rect frame) const;
  Size measure(LayoutConstraints const& constraints, LayoutHints const& hints) const;
};

} // namespace flux
```

The default `Element::Model<C>` detects `RenderComponent` via concept and handles layout node creation and scene emission automatically.

### Option C: Primitive Component (Layout Container)

Layout containers require an explicit implementation of all three methods: `layout(LayoutContext&)`, `measure(LayoutContext&, LayoutConstraints, LayoutHints, TextSystem&)`, and `renderFromLayout(RenderContext&, LayoutNode const&)`. Use `ContainerLayoutScope` and `ContainerMeasureScope` from `src/UI/Layout/ContainerScope.hpp`.

**Step 1: Define the struct**

```cpp
// include/Flux/UI/Views/WrapStack.hpp
struct WrapStack {
  float spacing = 8.f;
  float lineSpacing = 8.f;
  bool clip = false;
  std::vector<Element> children;
};
```

**Step 2: Implement measure, layout, and renderFromLayout**

```cpp
// src/UI/Layout/LayoutWrapStack.cpp
#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/Views/WrapStack.hpp>
#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

namespace flux {
using namespace flux::layout;

Size WrapStack::measure(LayoutContext& ctx, LayoutConstraints const& constraints,
                        LayoutHints const&, TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  // ... measure all children, compute wrap layout size
}

void WrapStack::layout(LayoutContext& ctx) const {
  ContainerLayoutScope scope(ctx);
  float const assignedW = stackMainAxisSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = stackMainAxisSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(clip, assignedW, assignedH);

  auto sizes = scope.measureChildren(children, childCs);

  for (std::size_t i = 0; i < children.size(); ++i) {
    scope.layoutChild(children[i], Rect{x, y, sz.width, sz.height}, childCs);
    // ... advance x, wrap to next line
  }
}

void WrapStack::renderFromLayout(RenderContext&, LayoutNode const&) const {
  // Containers emit a LayerNode generically in renderLayoutTree; nothing needed here.
}

} // namespace flux
```

### The Layout Container Contract

`ContainerLayoutScope` encapsulates:

1. **Slot consumption**: `consumeCompositeBodySubtreeRootSkip()` or `advanceChildSlot()` at scope construction.
2. **Layer node**: `pushStandardLayer(clip, w, h)` / `pushOffsetScrollLayer` / `pushScaleAroundCenterLayer` — creates a `LayoutNode::Container` with `ContainerLayerSpec`. The container node is recorded via `pushLayoutNode` **before** the container's layer world transform is pushed, so `LayoutNode::worldBounds` correctly reflects the parent-space position (using the parent's world transform). The layer transform is then pushed immediately after, establishing the coordinate system for all children. The destructor pops the layout parent and layer world transform.
3. **Child index management**: `pushChildIndex()` / `popChildIndex()` at construction/destruction.
4. **Measure-then-layout**: `scope.measureChildren(children, cs)` measures and rewinds automatically. Then `scope.layoutChild(child, frame, cs)` calls `child.layout(ctx)` with the assigned frame.

---

## Key Invariants

- `measure` must be **side-effect free**: no scene graph writes, no LayoutTree writes, no state mutations. It only returns a `Size` and advances child slots.
- `layout` writes only to the `LayoutTree` (via `ctx.pushLayoutNode`). It does NOT write to the SceneGraph or EventMap.
- `renderFromLayout` writes to the SceneGraph and EventMap. It does NOT call `LayoutContext` APIs.
- **`pushLayoutNode` before `pushLayerWorldTransform`**: for container nodes, `ctx.pushLayoutNode` must be called while `currentLayerWorldTransform()` still reflects the **parent's** transform. `ctx.pushLayerWorldTransform(local)` is called immediately after, so that children see the container's coordinate system. Reversing this order causes `LayoutNode::worldBounds` to be double-translated, breaking popover anchoring and any other feature that reads `LayoutRectCache`.
- The measure pass and layout pass within a container must visit children in the **same order** and advance the **same child indices** — this is why `rewindChildKeyIndex` exists.
- `MeasureCache` is keyed by `(elementMeasureId, constraints, hints)` and cleared every rebuild.
- Flex distribution only activates when the parent provides a **finite** main-axis size.

---

## File Map

| Area | Key Files |
|------|-----------|
| Type erasure | `include/Flux/UI/Element.hpp`, `src/UI/Element.cpp` |
| Concepts | `include/Flux/UI/Component.hpp` |
| Geometry | `include/Flux/Core/Types.hpp` |
| Constraints + hints | `include/Flux/UI/LayoutEngine.hpp` |
| Alignment | `include/Flux/Graphics/TextLayoutOptions.hpp` |
| Orchestrator | `include/Flux/UI/BuildOrchestrator.hpp`, `src/UI/BuildOrchestrator.cpp` |
| Layout context (Phase 1) | `include/Flux/UI/LayoutContext.hpp`, `src/UI/LayoutContext.cpp` |
| Render context (Phase 2) | `include/Flux/UI/RenderContext.hpp`, `src/UI/RenderContext.cpp` |
| Layout tree | `include/Flux/UI/LayoutTree.hpp`, `src/UI/LayoutTree.cpp` |
| Render walk | `include/Flux/UI/RenderLayoutTree.hpp`, `src/UI/RenderLayoutTree.cpp` |
| Root entry | `include/Flux/Detail/RootHolder.hpp` |
| Leaf bounds | `include/Flux/UI/Detail/LeafBounds.hpp`, `src/UI/Detail/LeafBounds.cpp` |
| Measure cache | `include/Flux/UI/MeasureCache.hpp` |
| Layout rect cache | `include/Flux/UI/LayoutRectCache.hpp` |
| Container scope helpers | `src/UI/Layout/ContainerScope.hpp` |
| Flex helpers | `src/UI/Layout/LayoutHelpers.hpp` |
| VStack layout | `include/Flux/UI/Views/VStack.hpp`, `src/UI/Layout/LayoutVStack.cpp` |
| HStack layout | `include/Flux/UI/Views/HStack.hpp`, `src/UI/Layout/LayoutHStack.cpp` |
| ZStack layout | `include/Flux/UI/Views/ZStack.hpp`, `src/UI/Layout/LayoutZStack.cpp` |
| Scene renderer | `include/Flux/Scene/SceneRenderer.hpp`, `src/Scene/SceneRenderer.cpp` |
| Runtime | `include/Flux/Detail/Runtime.hpp`, `src/UI/Runtime.cpp` |
| Main loop | `src/Core/Application.mm` |
