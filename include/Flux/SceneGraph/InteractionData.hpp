#pragma once

/// \file Flux/SceneGraph/InteractionData.hpp
///
/// Interaction payload attached directly to scenegraph nodes.

#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>

#include <functional>
#include <string>

namespace flux::scenegraph {

struct InteractionData {
  ComponentKey stableTargetKey{};
  Cursor cursor = Cursor::Inherit;
  bool focusable = false;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
  std::function<void(Vec2)> onScroll;
  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(KeyCode, Modifiers)> onKeyUp;
  std::function<void(std::string const&)> onTextInput;
  std::function<void()> onTap;

  [[nodiscard]] bool isEmpty() const noexcept {
    return !onPointerDown && !onPointerUp && !onPointerMove && !onScroll && !onKeyDown && !onKeyUp &&
           !onTextInput && !onTap && !focusable && cursor == Cursor::Inherit;
  }
};

class SceneNode;

struct InteractionHitResult {
  SceneNode const* node = nullptr;
  Point localPoint{};
  InteractionData const* interaction = nullptr;
};

} // namespace flux::scenegraph
