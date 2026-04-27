#pragma once

/// \file Flux/SceneGraph/InteractionData.hpp
///
/// Interaction payload attached directly to scenegraph nodes.

#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Reactive/Signal.hpp>

#include <functional>
#include <string>

namespace flux::scenegraph {

struct InteractionData {
  ComponentKey stableTargetKey{};
  Cursor cursor = Cursor::Inherit;
  bool focusable = false;
  std::function<void()> onPointerEnter;
  std::function<void()> onPointerExit;
  std::function<void()> onFocus;
  std::function<void()> onBlur;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
  std::function<void(Vec2)> onScroll;
  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(KeyCode, Modifiers)> onKeyUp;
  std::function<void(std::string const&)> onTextInput;
  std::function<void()> onTap;
  Reactive::Signal<bool> hoverSignal;
  Reactive::Signal<bool> pressSignal;
  Reactive::Signal<bool> focusSignal;
  Reactive::Signal<bool> keyboardFocusSignal;

  [[nodiscard]] bool isEmpty() const noexcept {
    return !onPointerEnter && !onPointerExit && !onFocus && !onBlur && !onPointerDown &&
           !onPointerUp && !onPointerMove && !onScroll && !onKeyDown && !onKeyUp &&
           !onTextInput && !onTap && hoverSignal.disposed() && pressSignal.disposed() &&
           focusSignal.disposed() && keyboardFocusSignal.disposed() && !focusable &&
           cursor == Cursor::Inherit;
  }
};

class SceneNode;

struct InteractionHitResult {
  SceneNode const* node = nullptr;
  Point localPoint{};
  InteractionData const* interaction = nullptr;
};

} // namespace flux::scenegraph
