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

/// UTF-8 text in a view box with optional background, border, and rounded corners.
/// Size comes from \c width / \c height when set, or from layout constraints (e.g. full window width when both are 0).
/// \c offsetX / \c offsetY shift the text box within the layout cell (same convention as \c Rectangle).
struct Text : ViewModifiers<Text> {
  std::string text;
  Font font{ .family = "", .size = 16.f, .weight = 400.f, .italic = false };

  FillStyle background = FillStyle::none();
  StrokeStyle border = StrokeStyle::none();
  Color color = Colors::black;

  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  TextWrapping wrapping = TextWrapping::Wrap;

  float padding = 0.f;
  CornerRadius cornerRadius{};

  float lineHeight = 0.f;
  int maxLines = 0;
  float firstBaselineOffset = 0.f;

  float offsetX = 0.f;
  float offsetY = 0.f;
  float width = 0.f;
  float height = 0.f;

  float flexGrow = 0.f;
  /// Defaults to 0 (unlike CSS `flex-shrink: 1`) so text layout is stable unless opted in.
  float flexShrink = 0.f;
  float minMainSize = 0.f;

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
