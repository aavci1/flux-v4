#pragma once

/// \file Flux/UI/Views/Text.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <functional>
#include <string>

namespace flux {

/// UTF-8 text in a view box. Size follows layout constraints; use \ref Element modifiers for
/// padding, frames, backgrounds, and flex.
struct Text : ViewModifiers<Text> {
  std::string text;
  Font font{ .family = "", .size = 16.f, .weight = 400.f, .italic = false };

  Color color = Colors::black;

  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  TextWrapping wrapping = TextWrapping::Wrap;

  float lineHeight = 0.f;
  int maxLines = 0;
  float firstBaselineOffset = 0.f;

  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
  /// When true, a PointerDown on this node claims focus for the window until another focusable node is tapped.
  bool focusable = false;
  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(KeyCode, Modifiers)> onKeyUp;
  std::function<void(std::string const&)> onTextInput;

  Cursor cursor = Cursor::Inherit;
};

} // namespace flux
