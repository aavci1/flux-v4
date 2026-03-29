# Event Queue

The event queue is the central dispatch path for application-visible work: window lifecycle, window chrome events, input, timer ticks, and user-defined payloads (`CustomEvent`). There is one queue per **`Application`**, owned by it and accessed via **`Application::instance().eventQueue()`**.

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
    TimerEvent,              // repeating timers scheduled via Application
    CustomEvent              // user-defined typed payload
>;
```

### WindowLifecycleEvent

Posted when a `Window` is constructed (registered) or destroyed (unregistered). Used so `Application` can track open windows.

- **`kind`:** `Registered` when the window is created, `Unregistered` when it is destroyed.
- **`handle`:** Stable platform window handle; valid for both kinds.
- **`window`:** Non-null **only** when `kind == Registered` (during construction); use it to associate the `Window*` with the handle before later events reference the handle alone.

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

### TimerEvent

Delivered when an **`Application::scheduleRepeatingTimer`** interval elapses. The **`Application::exec()`** main loop advances timers with **`std::chrono::steady_clock`**, posts **`TimerEvent`**s, then **`dispatch()`** runs handlers (same thread as `post`). Use **`Application::cancelTimer`** with **`timerId`** to stop.

```cpp
struct TimerEvent {
    std::int64_t deadlineNanos;   // steady_clock instant when delivered (ns since steady_clock epoch)
    std::uint64_t timerId;        // id from scheduleRepeatingTimer / cancelTimer
    unsigned int windowHandle;    // optional; 0 if not tied to a window
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

Pending events are stored in buckets and drained in order: **lifecycle → window → input → timer → custom**. Within each bucket, order is FIFO.

| Priority | Event type | Role |
|----------|------------|------|
| 1 | `WindowLifecycleEvent` | Registration changes before per-window events |
| 2 | `WindowEvent` | Geometry and window state |
| 3 | `InputEvent` | User input |
| 4 | `TimerEvent` | Periodic work (e.g. animations, clocks) before custom app events |
| 5 | `CustomEvent` | App-defined work |

---

## Thread safety

**Flux v4 today:** The queue does not enforce thread affinity; callers are expected to use **`post` / `dispatch` / `on`** only from the main thread. A future cross-thread `post` plus a platform wake is not implemented in the reference queue yet.

---

## Re-entrancy

- **`dispatch()`** sets a re-entrancy guard: a **nested** `dispatch()` (for example from inside an event handler) **returns immediately** without draining the queue again. Events posted during that handler are still enqueued and are processed when the **outer** `dispatch()` continues its drain loop.
- **`post`** is safe while a handler runs; new events are appended to the appropriate bucket under an internal mutex.

---

## Platform integration (main loop)

How the UI thread drives `dispatch()` depends on the platform:

- **macOS (current):** `Application::exec()` alternates platform event waits (`PlatformWindow::waitForEvents` / `processEvents`) with **`processDueTimers()`** (standard C++ `steady_clock` deadlines), **reactive / next-frame** bookkeeping (see below), and **`EventQueue::dispatch()`**. When idle, **`waitForEvents`** uses a timeout derived from the next timer deadline so the loop wakes for timer ticks without busy-waiting. Other platforms will attach the same contract to their event loop (e.g. Wayland `wl_display_dispatch`, KMS/DRM page-flip loop).

### Reactive updates and redraw

The **reactive** layer can mark work pending; **`Application::markReactiveDirty()`** (used internally when signals change) participates in the same main loop. After queued events are handled, the app may run **next-frame** callbacks registered with **`Application::onNextFrameNeeded()`** and coalesce redraws. This is separate from **`EventQueue`** buckets but still **main-thread**; custom UI events posted from those callbacks follow the usual **`post` → `dispatch`** path.

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
