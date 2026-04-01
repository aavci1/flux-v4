# Flux Layout System

## Architecture Overview

Flux uses a **type-erased, single-pass layout** model. There is no base `View` class — views are plain structs that satisfy one of two C++ concepts. A universal wrapper called `Element` type-erases them and dispatches `measure` and `build` through a virtual `Concept`/`Model<C>` pattern.

There is no separate global layout solver. Each layout container runs **measure children → flex distribute → set child frames → build children** inline during a single rebuild pass. Rendering happens afterward in a separate traversal of the resulting scene graph.

### Two Kinds of Components

Defined in `include/Flux/UI/Component.hpp`:

```cpp
// A composite has a body() that returns its subtree.
template<typename T>
concept CompositeComponent = requires(T const& t) { { t.body() }; };

// A render leaf draws directly and reports its own size.
template<typename T>
concept RenderComponent = requires(T const& t, Canvas& c, Rect r, LayoutConstraints const& cs) {
  { t.render(c, r) };
  { t.measure(cs) } -> std::convertible_to<Size>;
} && !CompositeComponent<T>;
```

**Composite components** (e.g. `Button`, `Slider`, `Toggle`) define `body()` returning an `Element`. They participate in layout indirectly — the framework measures and builds their `body()` subtree. They never draw or measure themselves.

**Render components** (e.g. `Rectangle`, `Text`, `PathShape`, `Line`) implement `render(Canvas&, Rect)` and `measure(LayoutConstraints const&) -> Size`. They are scene-graph leaves that draw directly and report their intrinsic size.

**Layout containers** (`VStack`, `HStack`, `ZStack`, `Grid`, `ScrollView`, `OffsetView`) are neither — they are plain structs with `std::vector<Element> children` that get **explicit `Element::Model<T>` specializations** in `Element.hpp` (generated via the `FLUX_ELEMENT_MODEL` macro), with their `build`/`measure` implemented in `src/UI/Layout/Layout*.cpp`.

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

  // Cross-axis alignment hints propagated by parent stacks:
  std::optional<VerticalAlignment>   hStackCrossAlign;  // set by HStack
  std::optional<HorizontalAlignment> vStackCrossAlign;  // set by VStack
};
```

Constraints flow top-down. The root constraints are the window size. Each container narrows constraints for its children (e.g. VStack sets `maxHeight = infinity` for children, HStack sets `maxWidth = infinity`).

### LayoutEngine (`include/Flux/UI/LayoutEngine.hpp`)

A minimal holder for the **current child frame** during the build pass:

```cpp
class LayoutEngine {
public:
  void resetForBuild();
  void setChildFrame(Rect frame);
  Rect consumeAssignedFrame();   // debug: asserts parent set frame for this child
  Rect lastAssignedFrame() const; // e.g. BuildOrchestrator::buildSlotRect after rebuild
};
```

Parent containers call `setChildFrame(...)` before building each child. Layout containers and leaves call `consumeAssignedFrame()` to read that assignment (debug builds assert the contract). `lastAssignedFrame()` returns the last stored rect without consuming the debug flag (used after a full pass for APIs like `buildSlotRect`).

### Layout stderr tree (`FLUX_DEBUG_LAYOUT`)

When the environment variable `FLUX_DEBUG_LAYOUT` is set to a non-empty value other than `0`/`false` (same truthiness as other layout diagnostics), each rebuild prints a text tree to **stderr**: opening/closing markers, then one line per container and leaf as the build walk visits nodes. Lines include **constraints** (`maxWidth`×`maxHeight`), **measured** size from the last `Element::measure` for that element, **frame** (`Rect`), and optional **flex** on leaves. Root containers may show `measured: 0×0` if no measure ran for that node before `build` (the root often goes straight to `buildInto`). Implementation: `include/Flux/UI/Detail/LayoutDebugDump.hpp`, `src/UI/LayoutDebugDump.cpp`, hooks in `Element::build`/`measure`, `BuildOrchestrator::rebuild`, and container `logContainer` calls after child measurement.

### Alignment (`include/Flux/Graphics/TextLayoutOptions.hpp`)

```cpp
enum class HorizontalAlignment : uint8_t { Leading, Center, Trailing };
enum class VerticalAlignment   : uint8_t { Top, Center, Bottom, FirstBaseline };
```

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

`Runtime` subscribes its rebuild function to `Application::onNextFrameNeeded`. Window resize events also trigger rebuild directly.

### 2. Rebuild (`BuildOrchestrator::rebuild`)

This is the heart of the layout system. One call does everything:

```
BuildOrchestrator::rebuild():
  1. graph.clear()                    // wipe the scene graph
  2. layoutEngine_.resetForBuild()    // clear stale child frame
  3. measureCache_.clear()            // fresh measure memoization
  4. stateStore_.beginRebuild()       // prepare reactive state slots

  5. Create BuildContext with:
     - scene graph, event map, text system, layout engine, measure cache

  6. Push root constraints (window width × height)
  7. rootHolder_->buildInto(ctx)      // recursive build pass
  8. layoutRects_.fill(graph, ctx)    // post-build rect cache for hooks
  9. stateStore_.endRebuild()
  10. Swap event maps, validate focus
  11. Rebuild overlays
  12. window_.requestRedraw()          // schedule render
```

### 3. The Build Pass (depth-first)

The build pass is a single depth-first traversal. For each node:

**Composite component** (`body()` exists):
1. Generate a `ComponentKey` for stable identity
2. Push state scope (`StateStore::pushComponent`)
3. Call `value.body()` to get the child `Element`
4. Pop state scope
5. Recursively `child.build(ctx)` on the body element

**Render leaf** (has `render` + `measure`):
1. `advanceChildSlot()` — consume a slot in the parent's child sequence
2. Resolve bounds from `childFrame()` + constraints via `resolveLeafBounds`
3. Add a `CustomRenderNode` to the scene graph with a draw lambda
4. Register event handlers in the `EventMap`

**Layout container** (VStack, HStack, ZStack, etc.):
1. `advanceChildSlot()` (unless composite body root)
2. Read `parentFrame` from `layoutEngine().consumeAssignedFrame()`
3. Read constraints from `ctx.constraints()`
4. Add a `LayerNode` (transform + optional clip) to the scene graph
5. **Measure pass**: iterate children, call `child.measure(ctx, childCs, textSystem)` collecting sizes
6. Rewind child key index (`ctx.rewindChildKeyIndex()`)
7. Compute flex distribution (grow/shrink) if the main axis is constrained
8. **Build pass**: for each child, `setChildFrame(...)`, push constraints, `child.build(ctx)`, pop constraints
9. Pop layer

The measure-then-build pattern within each container is critical: it allows the container to know all children's sizes before placing any of them.

### 4. Measure

`Element::measure` returns a `Size` without side effects (no scene graph writes). It uses `MeasureCache` for leaf memoization — keyed by `(elementMeasureId, constraints)`, cleared every rebuild.

For composites, measure recursively measures `body()`. For render leaves, it calls `value.measure(constraints)`. For layout containers, it measures all children and aggregates (sum along main axis for stacks, max for ZStack).

### 5. Render

After rebuild, `window_.requestRedraw()` schedules a paint. `Application::presentAllWindows()` calls `window->render(canvas)`, which invokes `SceneRenderer::render(SceneGraph, Canvas, clearColor)`.

`SceneRenderer` walks the scene graph depth-first from the root:
- `LayerNode` → push transform/clip, recurse into children, pop
- `RectNode` → draw filled/stroked rounded rect
- `TextNode` → draw laid-out text runs
- `ImageNode` → draw image
- `PathNode` → draw vector path
- `CustomRenderNode` → call the `draw` lambda (for generic `RenderComponent` leaves)

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
Element{MyView{...}}.withFlex(/*grow=*/1.f, /*shrink=*/1.f, /*minMain=*/50.f)
```

`Spacer` has implicit `flexGrow = 1.f` and `flexShrink = 0.f`.

Flex distribution follows CSS flexbox semantics:
- **Grow**: extra space distributed proportional to `flexGrow` weight
- **Shrink**: overflow removed proportional to `flexShrink × naturalSize`, iterating until converged (respecting `minSize` floors)

Flex only activates when the parent stack has a **finite** assigned main-axis size.

---

## How VStack / HStack / ZStack Work

All three follow the same pattern with axis-specific differences:

### VStack (vertical stack)

**Measure**: Sum of children heights + spacing + 2×padding. Width = max(child widths) + 2×padding, clamped to `maxWidth`.

**Build**:
1. Compute `innerW` from assigned width minus padding
2. Measure all children with `maxHeight = ∞`, `maxWidth = innerW`
3. Rewind key index
4. If height-constrained: flex grow/shrink on the Y axis
5. Place each child: `setChildFrame({padding, y, innerW, allocH[i]})`, advance `y`

Children get the **full column width** so nested HStacks can flex horizontally. Cross-axis alignment is communicated via `vStackCrossAlign` on the constraints (used by `Text` for glyph alignment).

### HStack (horizontal stack)

The horizontal dual of VStack:

**Measure**: Sum of children widths + spacing + 2×padding. Height = max(child heights) + 2×padding.

**Build**:
1. Measure all children with `maxWidth = ∞`
2. If width-constrained: flex grow/shrink on the X axis
3. Place each child: `setChildFrame({x, padding, allocW[i], rowInnerH})`, advance `x`

Children get the **full row height**. Cross-axis alignment uses `hStackCrossAlign`.

### ZStack (overlay stack)

All children share the same space:

**Measure**: `max(children widths) × max(children heights)`, expanded to at least the proposed size.

**Build**:
1. Measure all children
2. Compute `innerW/innerH` = max of (proposed, largest child)
3. For each child: compute alignment offset (`hAlign`, `vAlign`), `setChildFrame({x, y, alignW, alignH})`

---

## How to Implement a New Component

### Option A: Composite Component (recommended for most UI controls)

A composite component defines `body()` returning an `Element` built from existing primitives. This is the simplest path — no `Element::Model` specialization needed.

**Header** (`include/Flux/UI/Views/MySwitch.hpp`):

```cpp
#pragma once

#include <Flux/UI/Element.hpp>
#include <functional>

namespace flux {

struct MySwitch {
  bool isOn = false;
  std::function<void(bool)> onToggle;

  // Layout hints (optional, forwarded to the root element of body())
  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

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
  bool const hovered = useHover();

  return Element{ZStack{
    .hAlign = HorizontalAlignment::Center,
    .vAlign = VerticalAlignment::Center,
    .children = {
      Rectangle{
        .frame = {0, 0, 48, 24},
        .cornerRadius = CornerRadius{12},
        .fill = FillStyle::solid(isOn ? Colors::blue : Colors::gray),
        .onTap = [this] { if (onToggle) onToggle(!isOn); },
      },
      Text{
        .text = isOn ? "ON" : "OFF",
        .color = Colors::white,
      },
    },
  }};
}

} // namespace flux
```

That's it. The framework's default `Element::Model<C>` template handles composite components automatically — it calls `body()`, measures the resulting subtree, and builds it.

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
  Color fill = Colors::blue;

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  void render(Canvas& canvas, Rect frame) const;
  Size measure(LayoutConstraints const& constraints) const;
};

} // namespace flux
```

**Implementation** (`src/UI/Views/CircleView.cpp`):

```cpp
#include <Flux/UI/Views/CircleView.hpp>

namespace flux {

Size CircleView::measure(LayoutConstraints const& constraints) const {
  float d = radius * 2.f;
  return {d, d};
}

void CircleView::render(Canvas& canvas, Rect frame) const {
  // Draw a circle centered in the given frame
  canvas.drawCircle(frame.center(), radius, fill);
}

} // namespace flux
```

The default `Element::Model<C>` template detects `RenderComponent` via the concept and handles `build`/`measure` automatically:
- `measure` calls `value.measure(constraints)` and returns the size
- `build` calls `advanceChildSlot()`, resolves bounds, adds a `CustomRenderNode` that calls `value.render(canvas, frame)`, and registers any event handlers found on the struct (e.g. `onTap`, `onPointerDown`)

Render leaves support these optional event handler fields (detected via `requires` expressions):
- `onTap`, `onPointerDown`, `onPointerUp`, `onPointerMove`, `onScroll`
- `onKeyDown`, `onKeyUp`, `onTextInput`
- `cursor`, `cursorPassthrough`, `focusable`

---

## How to Implement a New Layout Container

Layout containers require an **explicit `Element::Model<T>` specialization** because they manage children, run measure-then-build internally, and manipulate the layout engine directly.

### Step 1: Define the struct

**Header** (`include/Flux/UI/Views/WrapStack.hpp`):

```cpp
#pragma once

#include <Flux/UI/Element.hpp>
#include <vector>

namespace flux {

struct WrapStack {
  float spacing = 8.f;
  float lineSpacing = 8.f;
  float padding = 0.f;
  bool clip = false;
  std::vector<Element> children;
};

} // namespace flux
```

### Step 2: Declare the `Element::Model` specialization

Add to `include/Flux/UI/Element.hpp` alongside the other specializations using the `FLUX_ELEMENT_MODEL` macro:

```cpp
FLUX_ELEMENT_MODEL(WrapStack);
```

The macro generates the full `Element::Model<WrapStack>` specialization: `clone()`, `build()`/`measure()` declarations, and standard flex delegates (`flexGrowOf`/`flexShrinkOf`/`minMainSizeOf` — these use SFINAE to read `flexGrow`/`flexShrink`/`minSize` fields if they exist on the struct, defaulting to 0).

For leaf types that support measure memoization, pass the override in the variadic tail:

```cpp
FLUX_ELEMENT_MODEL(MyLeaf, bool canMemoizeMeasure() const override { return true; });
```

### Step 3: Implement `measure` and `build`

**Implementation** (`src/UI/Layout/LayoutWrapStack.cpp`):

Use the `ContainerBuildScope` and `ContainerMeasureScope` RAII helpers (defined in `src/UI/Layout/ContainerScope.hpp`) to handle the container protocol automatically — slot consumption, layer management, child index push/pop, measure-rewind, and per-child frame+constraints wrapping.

```cpp
#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

namespace flux {
using namespace flux::layout;

Size Element::Model<WrapStack>::measure(BuildContext& ctx,
                                        LayoutConstraints const& constraints,
                                        TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);

  float const maxW = std::isfinite(constraints.maxWidth)
                         ? constraints.maxWidth
                         : 0.f;
  float const innerW = std::max(0.f, maxW - 2.f * value.padding);

  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  float lineW = 0.f, totalH = 0.f, lineH = 0.f;
  bool firstOnLine = true;

  for (Element const& ch : value.children) {
    Size const s = ch.measure(ctx, childCs, ts);
    if (!firstOnLine && lineW + value.spacing + s.width > innerW && innerW > 0.f) {
      totalH += lineH + value.lineSpacing;
      lineW = 0.f;
      lineH = 0.f;
      firstOnLine = true;
    }
    if (!firstOnLine) lineW += value.spacing;
    lineW += s.width;
    lineH = std::max(lineH, s.height);
    firstOnLine = false;
  }
  totalH += lineH;

  float const w = (innerW > 0.f ? maxW : lineW + 2.f * value.padding);
  return {w, totalH + 2.f * value.padding};
}

void Element::Model<WrapStack>::build(BuildContext& ctx) const {
  ContainerBuildScope scope(ctx);
  float const assignedW = stackMainAxisSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = stackMainAxisSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(value.clip, assignedW, assignedH);

  float const innerW = std::max(0.f, assignedW - 2.f * value.padding);

  LayoutConstraints childCs = scope.outer;
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  auto sizes = scope.measureChildren(value.children, childCs);

  float x = value.padding, y = value.padding, lineH = 0.f;
  bool firstOnLine = true;

  for (std::size_t i = 0; i < value.children.size(); ++i) {
    Size const sz = sizes[i];
    if (!firstOnLine && x + value.spacing + sz.width > assignedW - value.padding
        && innerW > 0.f) {
      y += lineH + value.lineSpacing;
      x = value.padding;
      lineH = 0.f;
      firstOnLine = true;
    }
    if (!firstOnLine) x += value.spacing;

    scope.buildChild(value.children[i], Rect{x, y, sz.width, sz.height}, childCs);

    x += sz.width;
    lineH = std::max(lineH, sz.height);
    firstOnLine = false;
  }
}

} // namespace flux
```

### The Layout Container Contract

The `ContainerBuildScope` and `ContainerMeasureScope` RAII helpers (in `src/UI/Layout/ContainerScope.hpp`) handle the protocol automatically. The underlying steps they encapsulate are:

1. **Slot consumption**: `consumeCompositeBodySubtreeRootSkip()` or `advanceChildSlot()` at the top of both `build` and `measure`. *(Handled by both scope constructors.)*

2. **Layer management**: In `build`, add a `LayerNode` to the scene graph, `pushLayer`, and `popLayer` when done. *(Use `scope.pushStandardLayer(clip, w, h)` for a translated+clipped layer, or `scope.pushCustomLayer(layerId)` for custom transforms. Destructor calls `popLayer`.)*

3. **Child index management**: `pushChildIndex()` before iterating children, `popChildIndex()` after. *(Handled by scope constructor/destructor.)*

4. **Measure-then-build**: In `build`, first measure all children, then rewind keys + state cursors, then build. *(Use `scope.measureChildren(children, cs)` which measures and rewinds automatically.)*

5. **Child frame + constraints**: Before building each child, `setChildFrame` + `pushConstraints` / `popConstraints`. *(Use `scope.buildChild(child, frame, cs)`.)*

6. **Composite subtree root**: `registerCompositeSubtreeRootIfPending(layerId)` right after adding the layer node. *(Handled by `pushStandardLayer` / `pushCustomLayer`.)*

---

## Key Invariants

- `measure` must be **side-effect free**: no scene graph writes, no state mutations. It only returns a `Size` and advances child slots.
- `build` performs side effects: writes scene nodes, registers event handlers, may read/write reactive state (via hooks like `useHover`, `usePress`, `useAnimated`, etc.).
- The measure pass and build pass within a container must visit children in the **same order** and advance the **same child indices** — this is why `rewindChildKeyIndex` exists.
- `MeasureCache` is keyed by `(elementMeasureId, constraints)` and cleared every rebuild. It only caches render leaves (`canMemoizeMeasure() == true`), never composites or layout containers.
- Flex distribution only activates when the parent provides a **finite** main-axis size. Unconstrained axes (e.g. inside an unconstrained-height VStack) keep natural sizes.

---

## File Map

| Area | Key Files |
|------|-----------|
| Type erasure | `include/Flux/UI/Element.hpp`, `src/UI/Element.cpp` |
| Concepts | `include/Flux/UI/Component.hpp` |
| Geometry | `include/Flux/Core/Types.hpp` |
| Constraints | `include/Flux/UI/LayoutEngine.hpp` |
| Alignment | `include/Flux/Graphics/TextLayoutOptions.hpp` |
| Orchestrator | `include/Flux/UI/BuildOrchestrator.hpp`, `src/UI/BuildOrchestrator.cpp` |
| Build context | `include/Flux/UI/BuildContext.hpp` |
| Root entry | `include/Flux/Detail/RootHolder.hpp` |
| Leaf bounds | `include/Flux/UI/Detail/LeafBounds.hpp`, `src/UI/Detail/LeafBounds.cpp` |
| Measure cache | `include/Flux/UI/MeasureCache.hpp` |
| Layout rect cache | `include/Flux/UI/LayoutRectCache.hpp` |
| Flex helpers | `src/UI/Layout/LayoutHelpers.hpp` |
| VStack layout | `include/Flux/UI/Views/VStack.hpp`, `src/UI/Layout/LayoutVStack.cpp` |
| HStack layout | `include/Flux/UI/Views/HStack.hpp`, `src/UI/Layout/LayoutHStack.cpp` |
| ZStack layout | `include/Flux/UI/Views/ZStack.hpp`, `src/UI/Layout/LayoutZStack.cpp` |
| Scene renderer | `include/Flux/Scene/SceneRenderer.hpp`, `src/Scene/SceneRenderer.cpp` |
| Runtime | `include/Flux/Detail/Runtime.hpp`, `src/UI/Runtime.cpp` |
| Main loop | `src/Core/Application.mm` |
