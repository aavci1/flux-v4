# Flux v4 — codebase conventions

This document describes how the repository is organized and the patterns used consistently across the code. It reflects the current tree as of this writing.

## Project identity

- **Name / version:** Flux v4 (`CMakeLists.txt`: `project(flux VERSION 4.0.0 …)`).
- **Platforms (roadmap):** **macOS** is implemented today. **Linux desktop** (Wayland + Vulkan) and **embedded Linux** (KMS/DRM) are planned; CMake reserves `FLUX_PLATFORM` values for those backends. Non-macOS builds fail at configure time until a backend is added.
- **Library:** Static library `flux`, plus example `hello_world`.
- **Language:** **C++23** (`CMAKE_CXX_STANDARD 23`), extensions off (`CMAKE_CXX_EXTENSIONS OFF`).
- **Minimum macOS:** 11.0 (`CMAKE_OSX_DEPLOYMENT_TARGET`) when targeting macOS.

## Build system

- **CMake** minimum 3.25.
- **Platform selection:** Cache variable `FLUX_PLATFORM` — `AUTO` (default on Apple hosts resolves to `MACOS`), `MACOS`, or reserved `LINUX_WAYLAND` / `LINUX_KMS` for future use. Only `MACOS` builds successfully today.
- **Languages:** `CXX` everywhere; **`OBJCXX` enabled only for the macOS backend** (`enable_language(OBJCXX)` when `FLUX_PLATFORM_MACOS`). Future Linux targets can stay **CXX-only** for core + Wayland/Vulkan sources.
- **Sources:** Mix of `.cpp` (portable core) and **Objective-C++** (`.mm`) for Cocoa / AppKit on macOS (`Application.mm`, `Platform/Mac/MacMetalWindow.mm`).
- **Includes:** Public API under `include/`; private helpers under `src/` (e.g. `src/Core/PlatformWindowCreate.hpp`) with `target_include_directories(… PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)`.
- **Warnings:** `-Wall -Wextra -Wpedantic` on the `flux` target.
- **Optional logging:** `FLUX_ENABLE_DEFAULT_EVENT_LOGGING` (CMake `option`, default `OFF`) — when `ON`, the default `Application` event handlers print to stdout (useful for debugging).
- **Apple frameworks (linked privately on macOS):** Cocoa, QuartzCore, Metal, Foundation.

## Directory layout

| Path | Role |
|------|------|
| `include/Flux/` | Public headers; stable API surface. |
| `include/Flux.hpp` | Umbrella include re-exporting core headers. |
| `src/Core/` | Core implementation (`Application`, `Window`, `EventQueue`, `PlatformWindow.hpp`, `PlatformWindowCreate.hpp`, factory). |
| `src/Platform/Mac/` | macOS-specific windowing (`MacMetalWindow.mm`). |
| `src/Platform/` | Future: e.g. `Linux/Wayland/`, `Linux/Kms/` mirroring the Mac layout — one implementation of `detail::createPlatformWindow` per supported platform build. |
| `examples/` | Sample apps (e.g. `hello-world`). |
| `docs/` | Project documentation (this file). |

Public headers live under `Flux/Core/` (types, events, application, window, queue). The abstract `PlatformWindow` interface is **private** — [`src/Core/PlatformWindow.hpp`](src/Core/PlatformWindow.hpp) — used only when building the library.

**Factory rule:** `flux::detail::createPlatformWindow(WindowConfig)` is implemented in exactly one platform translation unit per build — **no** `#ifdef` platform branches inside portable core files such as `Window.cpp`.

## Namespace

- All library API lives in **`flux`**.
- **`flux::detail`** is reserved for implementation helpers not meant as public API (e.g. `detail::isEventAlternativeV` in `EventQueue.hpp`, `detail::createPlatformWindow` in `PlatformWindowCreate.hpp`, and internal helpers inside `.mm` translation units).

## Public vs private headers

- **Public:** Anything under `include/Flux/…` included by consumers.
- **Private:** Headers under `src/` (e.g. `PlatformWindow.hpp`, `PlatformWindowCreate.hpp`) are implementation details; they must not be required by external projects using only `include/`.

## Umbrella include

- Prefer **`#include <Flux.hpp>`** for apps that want the main surface area; it pulls `Application`, `EventQueue`, `Events`, `Types`, and `Window` (see `include/Flux.hpp`).
- Finer-grained includes (`<Flux/Core/…>`) are fine when dependencies should stay minimal.

## Private implementation (pimpl)

Public classes that carry hidden state use the **pimpl** pattern consistently:

- The opaque type is always named **`struct Impl`** (nested forward declaration in the public header).
- The owning pointer is always **`std::unique_ptr<Impl> d`** — the member name is **`d`**, not `d_` or `impl_`.
- **`struct Impl` is defined only in `.cpp` / `.mm`** files, so the public header stays free of heavy dependencies and private members.

Applies to: `Application`, `Window`, `EventQueue`, and the macOS `MacMetalPlatformWindow` implementation class.

`EventQueue` exposes template methods (`post` / `on`) in the header. Only **`Application`** may construct a queue (`friend class Application`); the app holds `std::unique_ptr<EventQueue>` and exposes it via **`Application::eventQueue()`**. Construction uses `new EventQueue()` in `Application.mm` (friendship applies to `Application` member functions, not to `std::make_unique`). Call sites that invoke methods on **`eventQueue()`** must include **`EventQueue.hpp`** (a forward declaration in **`Application.hpp`** is not enough for `post` / `dispatch`). The private section also holds `friend struct detail::EventQueueImplAccess`, `struct Impl`, and `std::unique_ptr<Impl> d`. Template bodies call `detail::EventQueueImplAccess` static methods (declared after the class, defined in the `.cpp`) so `Impl` stays incomplete in the header. Application code should not use `EventQueueImplAccess`. **`post` / `dispatch` / `on`** are **main-thread-only by contract** (not enforced at runtime).

## Rule of five

Types that own unique resources or singleton-like semantics **delete** copy and move operations when not intended to be copied or moved:

- `Application`, `Window`, and `EventQueue` declare deleted copy/move constructors and assignment operators in their public headers.

## Naming

- **Types:** `PascalCase` for classes and structs (`Window`, `WindowConfig`, `EventQueue`).
- **Functions / methods:** `camelCase` (`createWindow`, `eventQueue`, `handle` on `Window`).
- **Enumerations:** `enum class` with `PascalCase` enumerators where used (`WindowEvent::Kind::Resize`, `InputEvent::Kind::PointerDown`).
- **Constants:** File-local or internal constants often use a **`k` prefix** and `camelCase` remainder (e.g. `kLifecycle`, `kWindow`, `kInput`, `kCustom`, `kBucketCount` in `EventQueue.cpp`).
- **Impl members:** Fields inside `Impl` structs may use **trailing underscores** for private data (`eventQueue_`, `buckets_`, …), distinguishing them from public API without a `m_` prefix on the outer class (the outer class has almost no private data besides `d`).

## Types and aliases

Shared vocabulary lives in `Types.hpp` (`Size`, `Vec2`, time aliases, `MouseButton`, `KeyCode`, `Modifiers`, etc.).

## Events

- **`Event`** is `std::variant<` … `>` of the first-class event structs (`WindowLifecycleEvent`, `WindowEvent`, `InputEvent`, `CustomEvent`) — see `Events.hpp`.
- **`CustomEvent`** carries a `std::uint32_t type` and `std::any payload` for arbitrary user payloads; `EventQueue` maps non-framework types to `CustomEvent` via `typeid`-derived IDs.
- **`EventQueue`:** `post` / `dispatch` / `on` are **main-thread-only by convention**. Obtain the queue with **`Application::instance().eventQueue()`** (or **`app.eventQueue()`**).

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

- `examples/hello-world/main.cpp` builds against the `flux` static library and shows constructing `Application`, `Window` with designated-style `WindowConfig` (C++20 aggregate initialization), and `return app.exec()`.

## Git

- A `.gitignore` at the repo root excludes build trees (`build/`, `cmake-build-*/`), CMake artifacts, common IDE paths, and macOS artifacts.

---

When adding new features, prefer extending these patterns (pimpl with `Impl` + `d`, `flux::detail` for non-public helpers, main-thread discipline for `EventQueue`, platform code behind `PlatformWindow` + factory) so the codebase stays uniform and the public headers stay stable.
