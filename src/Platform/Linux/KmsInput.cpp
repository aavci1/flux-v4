#include "Platform/Linux/KmsPlatform.hpp"

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>

#include "Platform/Linux/Common/XkbState.hpp"

#include <libinput.h>
#include <linux/input-event-codes.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

bool debugKmsInput() {
  char const* value = std::getenv("FLUX_DEBUG_KMS");
  return value && *value && std::strcmp(value, "0") != 0;
}

} // namespace

void KmsApplication::handleInputDeviceAdded(libinput_device* device) {
  if (!device) return;
  ++inputDeviceCount_;
  if (libinput_device_config_tap_get_finger_count(device) > 0) {
    libinput_device_config_tap_set_enabled(device, LIBINPUT_CONFIG_TAP_ENABLED);
  }
  if (debugKmsInput()) {
    std::fprintf(stderr, "Flux KMS input: device added: %s [keyboard=%d pointer=%d touch=%d tap_fingers=%d]\n",
                 libinput_device_get_name(device),
                 libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_KEYBOARD),
                 libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_POINTER),
                 libinput_device_has_capability(device, LIBINPUT_DEVICE_CAP_TOUCH),
                 libinput_device_config_tap_get_finger_count(device));
    std::fflush(stderr);
  }
}

void KmsApplication::setPointerPosition(Point position) {
  if (KmsWindow* window = focusedWindow()) {
    pointerPos_ = window->clampPointer(position);
    window->moveCursor(pointerPos_);
  } else {
    pointerPos_ = position;
  }
}

void KmsApplication::routePointer(Point position, InputEvent::Kind kind, MouseButton button,
                                  Vec2 scrollDelta, bool preciseScrollDelta) {
  KmsWindow* window = focusedWindow();
  if (!window) return;
  pointerPos_ = window->clampPointer(position);
  window->moveCursor(pointerPos_);
  if (debugKmsInput()) {
    char const* kindName = kind == InputEvent::Kind::PointerMove ? "move" :
                           kind == InputEvent::Kind::PointerDown ? "down" :
                           kind == InputEvent::Kind::PointerUp ? "up" :
                           kind == InputEvent::Kind::Scroll ? "scroll" : "pointer";
    std::fprintf(stderr, "Flux KMS input: pointer %s at %.1f,%.1f button=%u mask=%u\n",
                 kindName, pointerPos_.x, pointerPos_.y, static_cast<unsigned int>(button),
                 static_cast<unsigned int>(pressedButtons_));
  }
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
  handlePendingVtSignal();
  pollActiveVt();
  handlePendingTerminateSignal();
  if (!isVtForeground()) return;
  if (!input_) return;
  int const dispatchResult = libinput_dispatch(input_);
  if (dispatchResult != 0 && debugKmsInput()) {
    std::fprintf(stderr, "Flux KMS input: libinput_dispatch returned %d\n", dispatchResult);
  }
  while (libinput_event* event = libinput_get_event(input_)) {
    switch (libinput_event_get_type(event)) {
    case LIBINPUT_EVENT_DEVICE_ADDED:
      handleInputDeviceAdded(libinput_event_get_device(event));
      break;
    case LIBINPUT_EVENT_DEVICE_REMOVED:
      if (debugKmsInput()) {
        libinput_device* device = libinput_event_get_device(event);
        std::fprintf(stderr, "Flux KMS input: device removed: %s\n",
                     device ? libinput_device_get_name(device) : "(unknown)");
      }
      break;
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
    case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
    case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
    case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS: {
      auto* pointer = libinput_event_get_pointer_event(event);
      Vec2 delta{};
      if (libinput_event_pointer_has_axis(pointer, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        delta.x = static_cast<float>(libinput_event_pointer_get_scroll_value(pointer,
                                                                             LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL));
      }
      if (libinput_event_pointer_has_axis(pointer, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        delta.y = static_cast<float>(libinput_event_pointer_get_scroll_value(pointer,
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
    case LIBINPUT_EVENT_TOUCH_DOWN:
    case LIBINPUT_EVENT_TOUCH_MOTION:
    case LIBINPUT_EVENT_TOUCH_UP: {
      auto* touch = libinput_event_get_touch_event(event);
      KmsWindow* window = focusedWindow();
      if (!window) break;
      Size size = window->currentSize();
      Point p{static_cast<float>(libinput_event_touch_get_x_transformed(touch,
                                                                        static_cast<std::uint32_t>(size.width))),
              static_cast<float>(libinput_event_touch_get_y_transformed(touch,
                                                                        static_cast<std::uint32_t>(size.height)))};
      if (libinput_event_get_type(event) == LIBINPUT_EVENT_TOUCH_DOWN) {
        pressedButtons_ |= 1u;
        routePointer(p, InputEvent::Kind::PointerMove);
        routePointer(p, InputEvent::Kind::PointerDown, MouseButton::Left);
      } else if (libinput_event_get_type(event) == LIBINPUT_EVENT_TOUCH_UP) {
        routePointer(pointerPos_, InputEvent::Kind::PointerUp, MouseButton::Left);
        pressedButtons_ &= static_cast<std::uint8_t>(~1u);
      } else {
        routePointer(p, InputEvent::Kind::PointerMove);
      }
      break;
    }
    default:
      if (debugKmsInput()) {
        std::fprintf(stderr, "Flux KMS input: ignored libinput event type %d\n",
                     static_cast<int>(libinput_event_get_type(event)));
      }
      break;
    }
    libinput_event_destroy(event);
  }
}

} // namespace flux
