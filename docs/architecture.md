# Flux v4 — architecture

This document describes how the major pieces fit together. It is meant to stay aligned with the current tree and CMake layout.

## Layers

```mermaid
flowchart TB
  subgraph app [Application code]
    Ex[Examples / your app]
  end
  subgraph core [Core — portable C++]
    App[Application]
    Win[Window]
    EQ[EventQueue]
    Types[Types / Events]
  end
  subgraph ui [UI — declarative]
    Comp[Component / Element]
    LE[LayoutEngine / Layout]
    RT[Runtime]
    SS[StateStore / Hooks]
  end
  subgraph scene [Scene — retained tree]
    SG[SceneGraph]
    SR[SceneRenderer]
    HT[HitTester]
  end
  subgraph react [Reactive]
    Sig[Signal / Computed]
    Anim[Animated / Transition]
    Obs[Observer]
  end
  subgraph gfx [Graphics — abstract + portable pieces]
    Can[Canvas]
    Path[Path / PathFlattener]
    TS[TextSystem]
    TLayout[TextLayout / AttributedString]
  end
  subgraph mac [macOS implementation]
    PWin[MacMetalPlatformWindow]
    MCan[MetalCanvas]
    CT[CoreTextSystem]
    GA[GlyphAtlas]
    Sh[Metal shaders — embedded metallib]
  end
  Ex --> App
  Ex --> Win
  Ex --> Can
  Ex --> TS
  Ex --> Comp
  App --> EQ
  App --> Win
  App --> Obs
  Win --> Can
  Win --> SG
  Win --> RT
  RT --> Comp
  RT --> LE
  RT --> SG
  RT --> HT
  RT --> SS
  Comp --> Sig
  SR --> Can
  Can --> MCan
  TS --> CT
  MCan --> GA
  CT --> GA
  PWin --> MCan
```

- **Core** (`src/Core/`, `include/Flux/Core/`): `Application`, `Window`, `EventQueue`, shared types and events. Portable except where it calls into `PlatformWindow` and graphics backends through virtual interfaces. `Application` also owns the process `TextSystem`, repeating timers, and **reactive scheduling**: `markReactiveDirty()`, `onNextFrameNeeded()`, and `flushRedraw()` for cases where the AppKit run loop does not pump the default mode (e.g. live resize).
- **UI** (`src/UI/`, `include/Flux/UI/`): Declarative **components** built from `Element` trees, **layout** (`LayoutEngine`, flex/grid stacks, `ScrollView`), **hooks** (`useState`, `useAnimated`, …) backed by **`StateStore`**, and **`Runtime`** — created when you call **`Window::setView`** (see `WindowUI.hpp`). Each rebuild measures and lays out, then mutates the window’s **`SceneGraph`**. Input is routed via **`EventMap`** and **`HitTester`** on that graph.
- **Scene** (`src/Scene/`, `include/Flux/Scene/`): Retained **`SceneGraph`** (`LayerNode`, `RectNode`, `TextNode`, `ImageNode`, `PathNode`, etc.), **`SceneRenderer`** (walks the tree and issues `Canvas` draws), **`HitTester`** for picking, optional **`SceneGraphDump`** for debugging. Apps can use the graph **imperatively** (see `scene_demo`) or get it populated **by the UI runtime** when using `setView`.
- **Reactive** (`src/Reactive/`, `include/Flux/Reactive/`): **`Signal`**, **`Computed`**, **`Animated`**, **`Transition`**, **`AnimationClock`**, **`Observer`** — fine-grained updates; integration with `Application` batches work and can request redraws when reactive state changes.
- **Platform** (`src/Platform/Mac/`): Cocoa / Metal window and surface wiring. `detail::createPlatformWindow` is implemented in one translation unit per platform build (no `#ifdef` branches inside portable `Window.cpp`).
- **Graphics** (`src/Graphics/`, `include/Flux/Graphics/`): Abstract `Canvas` API, CPU-side `Path` and flattening, `TextSystem` with box and unconstrained layout helpers in `TextSystem.cpp`. Metal-specific code lives under `src/Graphics/Metal/` (rasterizer, device resources, shader library, glyph atlas, image textures, frame recording).

## Application and main loop

- **`Application::exec()`** runs the platform event loop. On macOS it alternates waiting for AppKit events, advancing **repeating timers** (`std::chrono::steady_clock`), posting **`TimerEvent`** to the queue, running **reactive / next-frame** work, and calling **`EventQueue::dispatch()`**.
- **`Window::requestRedraw()`** (and **`Application::requestRedraw()`**) mark frames needed; when the pump runs, **`Window::render(Canvas&)`** is invoked with **`beginFrame` / `present`** wrapped by `Application` so subclasses only draw. The default implementation clears via **`SceneRenderer`** when a **`SceneGraph`** exists (created lazily by **`sceneGraph()`** or by the UI **`Runtime`**).
- **`Application::textSystem()`** returns the process-wide **`TextSystem`** (macOS: **`CoreTextSystem`**) used for measurement, layout, and glyph rasterization consumed by the Metal **`GlyphAtlas`**.
- **`Application::flushRedraw()`** presents immediately — used when nested run-loop modes would otherwise defer redraw (documented on the API).

## Declarative UI and scene graph

- **`Window::setView(...)`** (templates in **`WindowUI.hpp`**) installs a root **component** and drives **`Runtime`**: rebuild passes resolve **`Element`** trees, run layout, sync **`SceneGraph`**, and register hit targets.
- Low-level drawing still goes through **`Canvas`**; the scene graph is the bridge between layout and GPU-backed primitives.

## Canvas and Metal

- **`Window::canvas()`** lazily creates a **`Canvas`** implementation (**`MetalCanvas`**) sized to the window’s drawable.
- Drawing primitives include transforms, clip rects, opacity, blend modes, rects/lines/paths/circles, images, and **`drawTextLayout`** for laid-out text.
- **Paths** are flattened on the CPU, then tessellated with **libtess2** in **`MetalPathRasterizer`** for fill/stroke meshes.
- **Shaders**: `CanvasShaders.metal` is compiled at build time to a **metallib**, embedded as a C array (`xxd`), and loaded from **`MetalShaderLibrary`**.

## Text

- **`TextSystem`** (virtual) provides **`layout`**, **`measure`**, **`resolveFontId`**, and **`rasterizeGlyph`**. Default implementations in **`TextSystem.cpp`** add box-constrained layout and alignment on top of backend **`layout`** / **`measure`**.
- **`CoreTextSystem.mm`** implements Core Text layout and glyph bitmap generation.
- **`GlyphAtlas.mm`** caches GPU textures for glyphs, calling back into **`TextSystem::rasterizeGlyph`** as needed.

## Dependencies (build / link)

| Dependency | Role |
|------------|------|
| **CMake FetchContent: libtess2** | Polygon tessellation for path fills |
| **Cocoa, QuartzCore, Metal, MetalKit, Foundation** | Windowing and GPU (macOS) |
| **CoreText** | Font resolution, shaping, rasterization for the text stack |

## Future platforms

CMake reserves **`FLUX_PLATFORM`** values for **Linux Wayland** and **KMS/DRM**; non-macOS **`AUTO`** fails at configure time until a backend exists. The intent is to keep **`PlatformWindow`** + **`createPlatformWindow`** as the extension point and to add parallel **`src/Platform/...`** trees without forking portable core logic.
