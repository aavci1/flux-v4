# Flux v5 Codebase Conventions

This document describes the current repository layout and coding patterns.

## Project Identity

- **Name / version:** Flux v5 (`CMakeLists.txt`: `project(flux VERSION 5.0.0 ...)`).
- **Platform:** macOS is implemented today. Linux desktop and embedded Linux values are reserved by `FLUX_PLATFORM` but fail at configure time until a backend is added.
- **Language:** C++23, extensions off.
- **Minimum macOS:** 11.0.
- **Library:** Static library target `flux`.
- **Examples:** Optional executable targets in [`examples/CMakeLists.txt`](../examples/CMakeLists.txt), enabled with `FLUX_BUILD_EXAMPLES=ON`.

## Build System

- CMake minimum is 3.25.
- C is enabled for vendored `libtess2`; Objective-C and Objective-C++ are enabled only for the macOS backend.
- Public includes come from `include/`; private implementation helpers come from `src/`.
- The `flux` target builds with `-Wall -Wextra -Wpedantic`.
- Metal shaders compile through `xcrun metal`, `metallib`, and `xxd` into an embedded shader header.

## Directory Layout

| Path | Role |
|------|------|
| `include/Flux/` | Public headers |
| `include/Flux.hpp` | Umbrella include |
| `include/Flux/Reactive/` | Signals, computed values, effects, scopes, bindings, animation |
| `include/Flux/UI/` | Declarative UI, hooks, views, layout, mount runtime |
| `include/Flux/SceneGraph/` | Retained scene tree and renderer-facing nodes |
| `include/Flux/Graphics/` | Canvas, text, path, and style types |
| `include/Flux/Detail/` | Implementation-facing public headers |
| `src/Core/` | `Application`, `Window`, event loop, platform window factory |
| `src/UI/` | Mount/runtime/layout implementation |
| `src/Reactive/` | Non-template reactive and animation implementation |
| `src/SceneGraph/` | Scene graph storage, traversal, hit testing, rendering |
| `src/Graphics/` | Portable graphics plus Metal/CoreText implementations |
| `src/Platform/Mac/` | macOS windowing |
| `examples/` | Sample apps |
| `docs/` | Documentation |

## Namespace

- Public API lives in `flux`.
- Reactive primitives live in `flux::Reactive`.
- `flux::detail` is reserved for implementation helpers not meant as app-facing API.

## Public And Private Headers

- Headers under `include/Flux/...` are public.
- Headers under `src/...` are private and must not be required by external consumers.
- Public headers must stay Objective-C-free.

## Umbrella Includes

- Use `#include <Flux.hpp>` for applications that want core, graphics, scene graph, and reactive primitives.
- Use `#include <Flux/UI/UI.hpp>` for declarative UI applications.
- Finer-grained includes are preferred inside library headers and tests when they reduce dependencies.

## Pimpl

Public owning classes that hide platform or implementation state use:

- nested forward declaration `struct Impl`;
- member `std::unique_ptr<Impl> d`;
- `Impl` definition only in `.cpp` or `.mm`.

This applies to `Application`, `Window`, `EventQueue`, macOS platform implementations, and `CoreTextSystem`.

## Naming

- Types: `PascalCase`.
- Functions and methods: `camelCase`.
- Constants: local constants often use `kName`.
- Private data in implementation structs may use trailing underscores.

## Retained UI

Flux v5 mounts UI once and updates retained scene nodes through reactive dependencies:

- `MountRoot` owns the root scene node and root `Reactive::Scope`.
- Hooks require an active owner scope.
- `Bindable<T>` modifiers install effects during mount.
- `For`, `Show`, and `Switch` manage dynamic subtree scopes.
- Environment values can be constants or reactive signals.

## Reactive UI

- Hooks that expose state return `Signal<T>`; read with `signal()` and write with
  `signal = value` or `signal.set(value)`.
- `.get()` remains available as an explicit synonym, but examples use `()` for
  read-and-subscribe sites.
- Use `.peek()` for intentional non-tracking reads.
- `useEnvironment<T>()` returns a signal for the active environment value. Read it
  inside `Bindable` closures or `Effect` bodies when UI should update after
  environment changes. A body-time read is a static mount-time seed and does not
  subscribe.

## Events

- `Event` is a variant of window lifecycle, window, input, timer, and custom events.
- `EventQueue::post`, `dispatch`, and `on` are main-thread-only by contract.
- Custom events use typed payloads wrapped in `CustomEvent`.
- Reactive work and next-frame callbacks are drained by `Application` on the same main loop as events.

## Platform Abstraction

`PlatformWindow` is private to `src/Core`. Portable core code calls `flux::detail::createPlatformWindow(WindowConfig)`, which is implemented by exactly one platform translation unit in a build.

## Includes

- Headers use `#pragma once`.
- Public project includes use angle brackets and paths relative to `include/`.
- Private includes from `src/` use paths relative to the `src` root.
- `.cpp` files include their corresponding public header first when there is one.

## Examples

Examples are intentionally small and are registered by `flux_add_example()` in [`examples/CMakeLists.txt`](../examples/CMakeLists.txt). Shared v5 example scaffolding lives in [`examples/common/V5ExampleApp.hpp`](../examples/common/V5ExampleApp.hpp).
