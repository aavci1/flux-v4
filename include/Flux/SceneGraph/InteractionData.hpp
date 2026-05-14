#pragma once

/// \file Flux/SceneGraph/InteractionData.hpp
///
/// Interaction payload attached directly to scenegraph nodes.

#include <Flux/Core/Identity.hpp>
#include <Flux/Core/Geometry.hpp>
#include <Flux/Reactive/Bindable.hpp>
#include <Flux/Reactive/SmallFn.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/SceneGraph/InteractionTypes.hpp>

#include <string>

namespace flux::scenegraph {

struct InteractionData {
  ComponentKey stableTargetKey{};
  Reactive::Bindable<Cursor> cursor{Cursor::Inherit};
  Reactive::Bindable<bool> focusable{false};
  Reactive::SmallFn<void()> onPointerEnter;
  Reactive::SmallFn<void()> onPointerExit;
  Reactive::SmallFn<void()> onFocus;
  Reactive::SmallFn<void()> onBlur;
  Reactive::SmallFn<void(Point, MouseButton)> onPointerDown;
  Reactive::SmallFn<void(Point, MouseButton)> onPointerUp;
  Reactive::SmallFn<void(Point)> onPointerMove;
  Reactive::SmallFn<void(Vec2)> onScroll;
  Reactive::SmallFn<void(KeyCode, Modifiers)> onKeyDown;
  Reactive::SmallFn<void(KeyCode, Modifiers)> onKeyUp;
  Reactive::SmallFn<void(std::string const&)> onTextInput;
  Reactive::SmallFn<void(MouseButton)> onTap;
  Reactive::Signal<bool> hoverSignal;
  Reactive::Signal<bool> pressSignal;
  Reactive::Signal<bool> focusSignal;
  Reactive::Signal<bool> keyboardFocusSignal;

  [[nodiscard]] bool isEmpty() const noexcept {
    return !onPointerEnter && !onPointerExit && !onFocus && !onBlur && !onPointerDown &&
           !onPointerUp && !onPointerMove &&
           !onScroll && !onKeyDown && !onKeyUp && !onTextInput && !onTap &&
           hoverSignal.disposed() && pressSignal.disposed() &&
           focusSignal.disposed() && keyboardFocusSignal.disposed() &&
           !focusable.isReactive() && !focusable.evaluate() &&
           !cursor.isReactive() && cursor.evaluate() == Cursor::Inherit;
  }
};

class SceneNode;

struct InteractionHitResult {
  SceneNode const* node = nullptr;
  Point localPoint{};
  InteractionData const* interaction = nullptr;
};

} // namespace flux::scenegraph
