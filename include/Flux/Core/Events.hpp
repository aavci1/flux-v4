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
  enum class Kind : std::uint8_t { Resize, FocusGained, FocusLost, DpiChanged, CloseRequest, Redraw };
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
  MouseButton button = MouseButton::None;
  KeyCode key = 0;
  Modifiers modifiers = Modifiers::None;
  std::string text;
};

struct CustomEvent {
  std::uint32_t type = 0;
  std::any payload;
};

using Event = std::variant<WindowLifecycleEvent, WindowEvent, InputEvent, CustomEvent>;

} // namespace flux
