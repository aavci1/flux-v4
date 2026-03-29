#pragma once

#include <Flux/Core/Types.hpp>

#include <any>
#include <cstdint>
#include <string>
#include <variant>

namespace flux {

class Window;

struct WindowLifecycleEvent {
  enum class Kind : std::uint8_t { Registered, Unregistered };
  Kind kind = Kind::Registered;
  /// Stable handle from the platform window; always valid for both kinds.
  unsigned int handle = 0;
  /// Valid only when `kind == Registered` (during `Window` construction).
  Window* window = nullptr;
};

struct WindowEvent {
  enum class Kind : std::uint8_t { Resize, FocusGained, FocusLost, DpiChanged, CloseRequest };
  Kind kind = Kind::Resize;
  unsigned int handle = 0;
  Size size{};
  float dpi = 1.0f;
};

struct InputEvent {
  enum class Kind : std::uint8_t {
    PointerMove,
    PointerDown,
    PointerUp,
    Scroll,
    KeyDown,
    KeyUp,
    TextInput,
    TouchBegin,
    TouchMove,
    TouchEnd
  };
  Kind kind = Kind::PointerMove;
  unsigned int handle = 0;
  Vec2 position{};
  /// Wheel / trackpad deltas when `kind == Scroll`; unused otherwise.
  Vec2 scrollDelta{};
  MouseButton button = MouseButton::None;
  KeyCode key = 0;
  Modifiers modifiers = Modifiers::None;
  std::string text;
};

/// Posted when an `Application`-scheduled repeating timer fires (main queue / run loop).
struct TimerEvent {
  /// Monotonic steady-clock instant when the timer delivered (nanoseconds since `steady_clock` epoch).
  std::int64_t deadlineNanos = 0;
  /// Opaque id from `Application::scheduleRepeatingTimer`; pass to `Application::cancelTimer`.
  std::uint64_t timerId = 0;
  /// Optional window association; `0` if none. Useful for routing redraws to a specific window.
  unsigned int windowHandle = 0;
};

struct CustomEvent {
  std::uint32_t type = 0;
  std::any payload;
};

using Event = std::variant<WindowLifecycleEvent, WindowEvent, InputEvent, TimerEvent, CustomEvent>;

} // namespace flux
