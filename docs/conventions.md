# Lambda v5 Codebase Conventions

This document describes the current repository layout and coding patterns.

## Project Identity

- **Name / version:** Lambda v5 (`CMakeLists.txt`: `project(lambda VERSION 5.0.0 ...)`).
- **Platform:** `LAMBDA_PLATFORM` selects one backend at build time: `MACOS`, `LINUX_WAYLAND`, or `LINUX_KMS`. `AUTO` picks macOS on Apple hosts and Wayland on Unix hosts.
- **Language:** C++23, extensions off.
- **Minimum macOS:** 11.0.
- **Library:** Static library target `lambda`.
- **Examples:** Optional executable targets in [`demos/CMakeLists.txt`](../demos/CMakeLists.txt), enabled with `LAMBDA_BUILD_EXAMPLES=ON`.

## Build System

- CMake minimum is 3.25.
- C is enabled for vendored `libtess2`; Objective-C and Objective-C++ are enabled only for the macOS backend.
- Public includes come from `include/`; private implementation helpers come from `src/`.
- The `lambda` target builds with `-Wall -Wextra -Wpedantic`.
- Metal shaders compile through `xcrun metal`, `metallib`, and `xxd` into an embedded shader header.

## Directory Layout

| Path | Role |
|------|------|
| `include/Lambda/` | Public headers |
| `include/Lambda.hpp` | Umbrella include |
| `include/Lambda/Reactive/` | Signals, computed values, effects, scopes, bindings, animation |
| `include/Lambda/UI/` | Declarative UI, hooks, views, layout, mount runtime |
| `include/Lambda/SceneGraph/` | Retained scene tree and renderer-facing nodes |
| `include/Lambda/Graphics/` | Canvas, text, path, and style types |
| `include/Lambda/Detail/` | Implementation-facing public headers |
| `src/Core/` | `Application`, `Window`, event loop, platform window factory |
| `src/UI/` | Mount/runtime/layout implementation |
| `src/Reactive/` | Non-template reactive and animation implementation |
| `src/SceneGraph/` | Scene graph storage, traversal, hit testing, rendering |
| `src/Graphics/` | Portable graphics plus Metal/CoreText implementations |
| `src/Platform/Mac/` | macOS windowing |
| `demos/` | Sample apps |
| `docs/` | Documentation |

## Namespace

- Public API lives in `lambda`.
- Reactive primitives live in `lambda::Reactive`.
- `lambda::detail` is reserved for implementation helpers not meant as app-facing API.

## Public And Private Headers

- Headers under `include/Lambda/...` are public.
- Headers under `src/...` are private and must not be required by external consumers.
- Public headers must stay Objective-C-free.

## Umbrella Includes

- Use `#include <Lambda.hpp>` for applications that want core, graphics, scene graph, and reactive primitives.
- Use `#include <Lambda/UI/UI.hpp>` for declarative UI applications.
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

Lambda v5 mounts UI once and updates retained scene nodes through reactive dependencies:

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
- `useEnvironment<Key>()` returns a signal for the active environment value. Read it
  inside `Bindable` closures or `Effect` bodies when UI should update after
  environment changes. A body-time read is a static mount-time seed and does not
  subscribe.

## Events

- `Event` is a variant of window lifecycle, window, input, timer, and custom events.
- `EventQueue::post`, `dispatch`, and `on` are main-thread-only by contract.
- Custom events use typed payloads wrapped in `CustomEvent`.
- Reactive work and next-frame callbacks are drained by `Application` on the same main loop as events.

## Platform Abstraction

`platform::Window` is private to `src/UI/Platform`. Portable UI code calls `lambda::platform::createWindow(WindowConfig)`, which is implemented by exactly one platform translation unit in a build.

## Includes

- Headers use `#pragma once`.
- Public project includes use angle brackets and paths relative to `include/`.
- Private includes from `src/` use paths relative to the `src` root.
- `.cpp` files include their corresponding public header first when there is one.

## Examples

Examples are intentionally small and are registered by `lambda_add_example()` in [`demos/CMakeLists.txt`](../demos/CMakeLists.txt). Shared v5 example scaffolding lives in [`demos/common/V5ExampleApp.hpp`](../demos/common/V5ExampleApp.hpp).
