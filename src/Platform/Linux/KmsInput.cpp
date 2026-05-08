#include "Platform/Linux/KmsPlatform.hpp"

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>

#include "Platform/Linux/Common/XkbState.hpp"

#include <libinput.h>
#include <linux/input-event-codes.h>

#include <cmath>
#include <memory>
#include <utility>

namespace flux {
namespace {

linux_platform::XkbState& xkbState() {
  static linux_platform::XkbState state;
  static bool initialized = state.createDefaultKeymap();
  (void)initialized;
  return state;
}

MouseButton mouseButtonFromLinux(std::uint32_t button) {
  if (button == BTN_LEFT) return MouseButton::Left;
  if (button == BTN_RIGHT) return MouseButton::Right;
  if (button == BTN_MIDDLE) return MouseButton::Middle;
  return MouseButton::Other;
}

std::uint8_t buttonMaskBit(std::uint32_t button) {
  if (button == BTN_LEFT) return 1u;
  if (button == BTN_RIGHT) return 2u;
  if (button == BTN_MIDDLE) return 4u;
  return 0u;
}

} // namespace

void KmsApplication::routePointer(Point position, InputEvent::Kind kind, MouseButton button,
                                  Vec2 scrollDelta, bool preciseScrollDelta) {
  KmsWindow* window = focusedWindow();
  if (!window) return;
  pointerPos_ = window->clampPointer(position);
  window->moveCursor(pointerPos_);
  Application::instance().eventQueue().post(InputEvent{.kind = kind,
                                                       .handle = window->handle(),
                                                       .position = pointerPos_,
                                                       .scrollDelta = scrollDelta,
                                                       .preciseScrollDelta = preciseScrollDelta,
                                                       .button = button,
                                                       .pressedButtons = pressedButtons_});
}

void KmsApplication::routeKey(std::uint32_t evdevKey, bool pressed) {
  KmsWindow* window = focusedWindow();
  if (!window) return;
  auto& xkb = xkbState();
  KeyCode const key = xkb.keyCodeForEvdevKey(evdevKey);
  Modifiers const modifiers = xkb.modifiers();
  xkb.updateKey(evdevKey, pressed);
  if (pressed && Application::instance().dispatchActionForShortcut(key, modifiers)) return;
  Application::instance().eventQueue().post(InputEvent{.kind = pressed ? InputEvent::Kind::KeyDown
                                                                        : InputEvent::Kind::KeyUp,
                                                       .handle = window->handle(),
                                                       .key = key,
                                                       .modifiers = modifiers});
  if (pressed) {
    std::string text = xkb.utf8ForEvdevKey(evdevKey);
    if (!text.empty()) {
      Application::instance().eventQueue().post(InputEvent{.kind = InputEvent::Kind::TextInput,
                                                           .handle = window->handle(),
                                                           .text = std::move(text)});
    }
  }
}

void KmsApplication::dispatchPendingInput() {
  if (!input_) return;
  libinput_dispatch(input_);
  while (libinput_event* event = libinput_get_event(input_)) {
    switch (libinput_event_get_type(event)) {
    case LIBINPUT_EVENT_POINTER_MOTION: {
      auto* pointer = libinput_event_get_pointer_event(event);
      pointerPos_.x += static_cast<float>(libinput_event_pointer_get_dx(pointer));
      pointerPos_.y += static_cast<float>(libinput_event_pointer_get_dy(pointer));
      routePointer(pointerPos_, InputEvent::Kind::PointerMove);
      break;
    }
    case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
      auto* pointer = libinput_event_get_pointer_event(event);
      KmsWindow* window = focusedWindow();
      if (!window) break;
      Size size = window->currentSize();
      Point p{static_cast<float>(libinput_event_pointer_get_absolute_x_transformed(pointer,
                                                                                  static_cast<std::uint32_t>(size.width))),
              static_cast<float>(libinput_event_pointer_get_absolute_y_transformed(pointer,
                                                                                  static_cast<std::uint32_t>(size.height)))};
      routePointer(p, InputEvent::Kind::PointerMove);
      break;
    }
    case LIBINPUT_EVENT_POINTER_BUTTON: {
      auto* pointer = libinput_event_get_pointer_event(event);
      std::uint32_t button = libinput_event_pointer_get_button(pointer);
      bool pressed = libinput_event_pointer_get_button_state(pointer) == LIBINPUT_BUTTON_STATE_PRESSED;
      std::uint8_t bit = buttonMaskBit(button);
      if (pressed) pressedButtons_ |= bit;
      else pressedButtons_ &= static_cast<std::uint8_t>(~bit);
      routePointer(pointerPos_, pressed ? InputEvent::Kind::PointerDown : InputEvent::Kind::PointerUp,
                   mouseButtonFromLinux(button));
      break;
    }
    case LIBINPUT_EVENT_POINTER_AXIS: {
      auto* pointer = libinput_event_get_pointer_event(event);
      Vec2 delta{};
      if (libinput_event_pointer_has_axis(pointer, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        delta.x = static_cast<float>(libinput_event_pointer_get_axis_value(pointer,
                                                                           LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL));
      }
      if (libinput_event_pointer_has_axis(pointer, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        delta.y = static_cast<float>(libinput_event_pointer_get_axis_value(pointer,
                                                                           LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL));
      }
      routePointer(pointerPos_, InputEvent::Kind::Scroll, MouseButton::None, delta, true);
      break;
    }
    case LIBINPUT_EVENT_KEYBOARD_KEY: {
      auto* keyboard = libinput_event_get_keyboard_event(event);
      bool pressed = libinput_event_keyboard_get_key_state(keyboard) == LIBINPUT_KEY_STATE_PRESSED;
      routeKey(libinput_event_keyboard_get_key(keyboard), pressed);
      break;
    }
    default:
      break;
    }
    libinput_event_destroy(event);
  }
}

} // namespace flux
