# Event Queue

The event queue is the central dispatch path for application-visible work: window lifecycle, window chrome events, input, and user-defined payloads (`CustomEvent`). There is one queue per **`Application`**, owned by it and accessed via **`Application::instance().eventQueue()`**.

---

## Design Goals

- **Unified** — one mechanism for framework events and custom payloads
- **Ordered** — events are processed in a defined priority order each `dispatch()`
- **Batched** — pending events drain in bucket order until the queue is empty
- **Main-thread contract** — `post` / `dispatch` / `on` are only valid from the application main thread (by convention; not runtime-checked)

---

## Event Types

The core type is a `std::variant` of first-class structs. User-defined types are wrapped in `CustomEvent` (see below).

```cpp
using Event = std::variant<
    WindowLifecycleEvent,  // window registered / unregistered with the app
    WindowEvent,             // resize, focus, DPI, close request
    InputEvent,              // pointer, keyboard, touch, scroll
    CustomEvent              // user-defined typed payload
>;
```

### WindowLifecycleEvent

Posted when a `Window` is constructed (registered) or destroyed (unregistered). Used so `Application` can track open windows.

### WindowEvent

Resize, focus gained/lost, backing-scale (DPI) changes, and close requests.

```cpp
struct WindowEvent {
    enum class Kind { Resize, FocusGained, FocusLost, DpiChanged, CloseRequest };
    Kind    kind;
    unsigned int handle;
    Size    size;       // valid when kind == Resize
    float   dpi;        // valid when kind == DpiChanged
};
```

### InputEvent

Raw input from the platform layer. Pointer coordinates are in logical units where applicable.

```cpp
struct InputEvent {
    enum class Kind { PointerMove, PointerDown, PointerUp, Scroll,
                      KeyDown, KeyUp, TextInput, TouchBegin, TouchMove, TouchEnd };
    Kind        kind;
    unsigned int handle;
    Vec2        position;
    MouseButton button;
    KeyCode     key;
    Modifiers   modifiers;
    String      text;
};
```

### CustomEvent

Extension point for app-specific or domain events (e.g. networking results). Payloads are type-erased in `std::any`; `EventQueue::post<T>()` / `on<T>()` map types to a stable id derived from `typeid`.

```cpp
struct CustomEvent {
    std::uint32_t type;
    std::any      payload;
};
```

---

## Queue API

`EventQueue` is not publicly constructible; **`Application`** creates the queue. Use:

```cpp
Application::instance().eventQueue().post(...);
Application::instance().eventQueue().dispatch();
Application::instance().eventQueue().on<MyEvent>(...);
```

Public methods on **`EventQueue`** (via that reference):

```cpp
void post(Event event);

template<typename T>
void post(T&& value);   // CustomEvent payload or first-class alternative

template<typename T>
void on(std::function<void(T const&)> handler);

void dispatch();
```

---

## Dispatch order

Pending events are stored in buckets and drained in order: **lifecycle → window → input → custom**. Within each bucket, order is FIFO.

| Priority | Event type | Role |
|----------|------------|------|
| 1 | `WindowLifecycleEvent` | Registration changes before per-window events |
| 2 | `WindowEvent` | Geometry and window state |
| 3 | `InputEvent` | User input |
| 4 | `CustomEvent` | App-defined work |

---

## Thread safety

**Flux v4 today:** The queue does not enforce thread affinity; callers are expected to use **`post` / `dispatch` / `on`** only from the main thread. A future cross-thread `post` plus a platform wake is not implemented in the reference queue yet.

---

## Platform integration (main loop)

How the UI thread drives `dispatch()` depends on the platform:

- **macOS (current):** A `CFRunLoopObserver` on the main run loop calls `EventQueue::dispatch()` before sources each iteration; `Application::exec()` may call `dispatch()` before `[NSApp run]`. Other platforms will attach the same contract to their event loop (e.g. Wayland `wl_display_dispatch`, KMS/DRM page-flip loop).

---

## macOS: `post` vs `dispatch` from AppKit

Cocoa callbacks sometimes **only `post`** an event; the next **`dispatch()`** (from the run-loop observer or an explicit call) delivers it. Other paths **`post` and then `dispatch()` immediately** — for example `Window`’s constructor and destructor post `WindowLifecycleEvent` and call `dispatch()` so synchronous setup/teardown sees a drained queue. **Closing a window** posts `WindowEvent::CloseRequest` and dispatches right away so the application can react before returning to nested run-loop calls.

---

## Using the queue for external systems

Background work can post results via `CustomEvent` (typed `post<T>()`). The handler runs on the UI thread during `dispatch()`.

```cpp
struct DataReadyEvent {
    User user;
};

eventQueue.on<DataReadyEvent>([&](DataReadyEvent const& e) {
    // update UI
});
```

---

## Custom event type ids

`EventQueue` maps non-framework types to `CustomEvent` using a `std::uint32_t` derived from `std::type_index(typeid(T)).hash_code()`. In theory distinct types could collide; if custom events become central, consider a registry or explicit ids.
