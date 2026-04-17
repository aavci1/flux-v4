#pragma once

/// \file Flux/Scene/InteractionData.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/UI/ComponentKey.hpp>

#include <functional>
#include <string>

namespace flux {

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

} // namespace flux
