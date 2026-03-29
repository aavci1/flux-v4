#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <functional>
#include <string>

namespace flux {

/// UTF-8 text in a view box with optional background, border, and rounded corners.
/// Size comes from `frame` when set, or from layout constraints (e.g. full window width when `frame` is empty).
struct Text {
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

  Rect frame{};

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
};

} // namespace flux
