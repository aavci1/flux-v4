# Flux v4 — codebase conventions

This document describes how the repository is organized and the patterns used consistently across the code. It reflects the current tree as of this writing.

## Project identity

- **Name / version:** Flux v4 (`CMakeLists.txt`: `project(flux VERSION 4.0.0 …)`).
- **Platforms (roadmap):** **macOS** is implemented today. **Linux desktop** (Wayland + Vulkan) and **embedded Linux** (KMS/DRM) are planned; CMake reserves `FLUX_PLATFORM` values for those backends. Non-macOS builds fail at configure time until a backend is added.
- **Library:** Static library `flux`, plus optional example executables (default **OFF**) listed in [`examples/CMakeLists.txt`](../examples/CMakeLists.txt); enable with **`FLUX_BUILD_EXAMPLES=ON`**.
- **Language:** **C++23** (`CMAKE_CXX_STANDARD 23`), extensions off (`CMAKE_CXX_EXTENSIONS OFF`).
- **Minimum macOS:** 11.0 (`CMAKE_OSX_DEPLOYMENT_TARGET`) when targeting macOS.

## Build system

- **CMake** minimum 3.25. The top-level `project()` enables **CXX** and **C** (C is used by the vendored **libtess2** sources).
- **Platform selection:** Cache variable `FLUX_PLATFORM` — `AUTO` (default on Apple hosts resolves to `MACOS`), `MACOS`, or reserved `LINUX_WAYLAND` / `LINUX_KMS` for future use. Only `MACOS` builds successfully today.
- **Languages:** `CXX` everywhere; **`OBJCXX` enabled only for the macOS backend** (`enable_language(OBJCXX)` when `FLUX_PLATFORM_MACOS`). Future Linux targets can stay **CXX-only** for core + Wayland/Vulkan sources.
- **Sources:** Mix of `.cpp` (portable core) and **Objective-C++** (`.mm`) for Cocoa / AppKit on macOS (`Application.mm`, `Platform/Mac/MacMetalWindow.mm`).
- **Includes:** Public API under `include/`; private helpers under `src/` (e.g. `src/Core/PlatformWindowCreate.hpp`) with `target_include_directories(… PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)`.
- **Warnings:** `-Wall -Wextra -Wpedantic` on the `flux` target.
- **Optional logging:** `FLUX_ENABLE_DEFAULT_EVENT_LOGGING` (CMake `option`, default **`OFF`**) — set **`ON`** to print default `Application` event handlers to stdout (handy for debugging).
- **Examples:** `FLUX_BUILD_EXAMPLES` (default **`OFF`**) — when **`ON`**, the root project `add_subdirectory(examples)`; example targets are defined in [`examples/CMakeLists.txt`](../examples/CMakeLists.txt).
- **Apple frameworks (linked privately on macOS):** Cocoa, QuartzCore, Metal, MetalKit, Foundation, CoreText.
- **Third-party (FetchContent):** **libtess2** — path fill/stroke tessellation; pulled at configure time from GitHub.
- **Metal shaders:** `src/Graphics/Metal/CanvasShaders.metal` is compiled to a **metallib** and embedded into `MetalShaderLibrary.mm` via a CMake custom command (`xcrun metal` / `metallib` / `xxd`).

## Directory layout

| Path | Role |
|------|------|
| `include/Flux/` | Public headers; stable API surface. |
| `include/Flux.hpp` | Umbrella include: core, graphics, and [`Reactive.hpp`](../include/Flux/Reactive/Reactive.hpp). |
| `include/Flux/UI/` | Declarative UI (`Element`, layout, views, hooks, `LayoutEngine`). Entry point: [`UI.hpp`](../include/Flux/UI/UI.hpp). |
| `include/Flux/Scene/` | Scene graph, renderer, hit testing, node store. |
| `include/Flux/Reactive/` | Signals, computed values, animation, observers. |
| `include/Flux/Detail/` | Internal helpers (`Runtime.hpp`, `RootHolder.hpp`) — not stable public API. |
| `src/Core/` | Core implementation (`Application`, `Window`, `EventQueue`, `PlatformWindow.hpp`, `PlatformWindowCreate.hpp`, factory). |
| `src/UI/` | UI runtime, layout, element build, state store, event map. |
| `src/Scene/` | Scene graph storage, renderer, hit tester, optional dump. |
| `src/Reactive/` | Reactive primitives implementation. |
| `src/Graphics/` | Portable graphics (`Canvas.cpp`, `Path.cpp`, `PathFlattener`, `TextSystem.cpp`) and Metal/Core Text implementations (`Metal/`, `CoreTextSystem.mm`). |
| `src/Platform/Mac/` | macOS-specific windowing (`MacMetalWindow.mm`). |
| `src/Platform/` | Future: e.g. `Linux/Wayland/`, `Linux/Kms/` mirroring the Mac layout — one implementation of `detail::createPlatformWindow` per supported platform build. |
| `examples/` | Sample apps; CMake lists targets in [`examples/CMakeLists.txt`](../examples/CMakeLists.txt) (see [Examples](#examples)). |
| `docs/` | Project documentation (this file and companions). |

Public headers live under `Flux/Core/`, **`Flux/Graphics/`**, **`Flux/UI/`**, **`Flux/Scene/`**, and **`Flux/Reactive/`**. The abstract `PlatformWindow` interface is **private** — [`src/Core/PlatformWindow.hpp`](src/Core/PlatformWindow.hpp) — used only when building the library. Headers under **`Flux/Detail/`** are implementation-facing; prefer **`Window::setView`** and **`#include <Flux/UI/UI.hpp>`** rather than including detail headers directly.

**Factory rule:** `flux::detail::createPlatformWindow(WindowConfig)` is implemented in exactly one platform translation unit per build — **no** `#ifdef` platform branches inside portable core files such as `Window.cpp`.

## Namespace

- All library API lives in **`flux`**.
- **`flux::detail`** is reserved for implementation helpers not meant as public API (e.g. `detail::isEventAlternativeV` in `EventQueue.hpp`, `detail::createPlatformWindow` in `PlatformWindowCreate.hpp`, and internal helpers inside `.mm` translation units).

## Public vs private headers

- **Public:** Anything under `include/Flux/…` included by consumers.
- **Private:** Headers under `src/` (e.g. `PlatformWindow.hpp`, `PlatformWindowCreate.hpp`) are implementation details; they must not be required by external projects using only `include/`.

## Umbrella include

- Prefer **`#include <Flux.hpp>`** for apps that want the main surface area; it pulls `Application`, `EventQueue`, `Events`, `Types`, `Window`, the **`Reactive`** umbrella, **`Canvas`**, and **`Styles`** (see `include/Flux.hpp`). Graphics-only consumers can include **`TextSystem.hpp`**, **`Path.hpp`**, etc., directly under `Flux/Graphics/`.
- Declarative UI: **`#include <Flux/UI/UI.hpp>`** (and `WindowUI.hpp` is included there for **`Window::setView`**). Scene-only or imperative demos can include the retained scene tree headers under **`Flux/Scene/`** without the UI layer.
- Finer-grained includes (`<Flux/Core/…>`) are fine when dependencies should stay minimal.

## Private implementation (pimpl)

Public classes that carry hidden state use the **pimpl** pattern consistently:

- The opaque type is always named **`struct Impl`** (nested forward declaration in the public header).
- The owning pointer is always **`std::unique_ptr<Impl> d`** — the member name is **`d`**, not `d_` or `impl_`.
- **`struct Impl` is defined only in `.cpp` / `.mm`** files, so the public header stays free of heavy dependencies and private members.

Applies to: `Application`, `Window`, `EventQueue`, the macOS `MacMetalPlatformWindow` implementation class, and **`CoreTextSystem`** (concrete `TextSystem`).

`EventQueue` exposes template methods (`post` / `on`) in the header. Only **`Application`** may construct a queue (`friend class Application`); the app holds `std::unique_ptr<EventQueue>` and exposes it via **`Application::eventQueue()`**. Construction uses `new EventQueue()` in `Application.mm` (friendship applies to `Application` member functions, not to `std::make_unique`). Call sites that invoke methods on **`eventQueue()`** must include **`EventQueue.hpp`** (a forward declaration in **`Application.hpp`** is not enough for `post` / `dispatch`). The private section also holds `friend struct detail::EventQueueImplAccess`, `struct Impl`, and `std::unique_ptr<Impl> d`. Template bodies call `detail::EventQueueImplAccess` static methods (declared after the class, defined in the `.cpp`) so `Impl` stays incomplete in the header. Application code should not use `EventQueueImplAccess`. **`post` / `dispatch` / `on`** are **main-thread-only by contract** (not enforced at runtime).

## Rule of five

Types that own unique resources or singleton-like semantics **delete** copy and move operations when not intended to be copied or moved:

- `Application`, `Window`, and `EventQueue` declare deleted copy/move constructors and assignment operators in their public headers.

## Naming

- **Types:** `PascalCase` for classes and structs (`Window`, `WindowConfig`, `EventQueue`).
- **Functions / methods:** `camelCase` (`createWindow`, `eventQueue`, `handle` on `Window`).
- **Enumerations:** `enum class` with `PascalCase` enumerators where used (`WindowEvent::Kind::Resize`, `InputEvent::Kind::PointerDown`).
- **Constants:** File-local or internal constants often use a **`k` prefix** and `camelCase` remainder (e.g. `kLifecycle`, `kWindow`, `kInput`, `kTimer`, `kCustom`, `kBucketCount` in `EventQueue.cpp`).
- **Impl members:** Fields inside `Impl` structs may use **trailing underscores** for private data (`eventQueue_`, `buckets_`, …), distinguishing them from public API without a `m_` prefix on the outer class (the outer class has almost no private data besides `d`).

## Types and aliases

Shared vocabulary lives in `Types.hpp` (`Size`, `Vec2`, time aliases, `MouseButton`, `KeyCode`, `Modifiers`, etc.).

## Events

- **`Event`** is `std::variant<WindowLifecycleEvent, WindowEvent, InputEvent, TimerEvent, CustomEvent>` — see `Events.hpp`.
- **`CustomEvent`** carries a `std::uint32_t type` and `std::any payload` for arbitrary user payloads; `EventQueue` maps non-framework types to `CustomEvent` via `typeid`-derived IDs.
- **`EventQueue`:** `post` / `dispatch` / `on` are **main-thread-only by convention**. Obtain the queue with **`Application::instance().eventQueue()`** (or **`app.eventQueue()`**).
- **`TextSystem`:** obtain with **`Application::instance().textSystem()`** for layout, measurement, and font/glyph resolution used by **`Canvas::drawTextLayout`** and the Metal glyph atlas.
- **`Application`:** reactive integration includes **`markReactiveDirty()`** (internal / reactive layer) and **`onNextFrameNeeded()`** for batched work once per main-loop iteration after reactive updates.

## Platform abstraction

- **`PlatformWindow`** ([`src/Core/PlatformWindow.hpp`](src/Core/PlatformWindow.hpp)) is an internal abstract interface: sizing, title, fullscreen, native presentation surface (`nativeGraphicsSurface()`), `setFluxWindow(Window*)`, etc. Not installed as a public header.
- **Factory:** `flux::detail::createPlatformWindow(WindowConfig)` returns `std::unique_ptr<PlatformWindow>`; implemented in the platform translation unit (e.g. `Platform/Mac/MacMetalWindow.mm` on macOS).
- Cocoa/AppKit types and Objective-C categories/implementations stay inside **`.mm` files**; public C++ headers stay ObjC-free.

## Includes

- Headers use **`#pragma once`** guard.
- Project headers use **angle brackets** and paths relative to `include/` — e.g. `#include <Flux/Core/Events.hpp>`.
- Private includes from `src/` use paths relative to the `src` root — e.g. `#include "Core/PlatformWindow.hpp"` / `#include "Core/PlatformWindowCreate.hpp"` from any `.cpp` / `.mm` under `src/`.
- Order is typically: corresponding public header first (in `.cpp`), then other Flux headers, then standard library, then system/framework (in `.mm`).

## Documentation comments

- Public APIs use **`///` Doxygen-style** line comments where extra clarity is needed (e.g. `EventQueue`, `Window`).

## Examples

| Directory | Demonstrates |
|-----------|----------------|
| `examples/hello-world` | Minimal `Application`, `Window`, `WindowConfig`, `app.exec()` |
| `examples/clock-demo` | Timers, redraw requests, `Window::render` / canvas drawing |
| `examples/blend-demo` | Opacity and blend modes on the canvas |
| `examples/text-demo` | `TextSystem`, `AttributedString`, `TextLayout`, `Canvas::drawTextLayout` |
| `examples/image-demo` | `loadImageFromFile`, `Image`, `Canvas::drawImage` |
| `examples/reactive-demo` | `Signal`, `Computed`, `Animation`, `Observer` with canvas |
| `examples/animation-demo` | `useAnimation`, `WithTransition`, repeat/autoreverse, reduced motion |
| `examples/card-demo` | `setView`, `VStack` / `HStack`, hooks, `useAnimation`, interactions |

Each target links the `flux` static library; see [`examples/CMakeLists.txt`](../examples/CMakeLists.txt) for CMake target names (`hello_world`, `clock_demo`, …).

## Git

- A `.gitignore` at the repo root excludes build trees (`build/`, `cmake-build-*/`), CMake artifacts, common IDE paths, and macOS artifacts.

---

When adding new features, prefer extending these patterns (pimpl with `Impl` + `d`, `flux::detail` for non-public helpers, main-thread discipline for `EventQueue`, platform code behind `PlatformWindow` + factory, graphics behind `Canvas` + `TextSystem`, UI rebuilds and layout behind **`Runtime`** + **`LayoutEngine`**) so the codebase stays uniform and the public headers stay stable.
