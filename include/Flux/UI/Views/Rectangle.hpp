#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <functional>

namespace flux {

struct Rectangle {
  Rect frame{};
  CornerRadius cornerRadius{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  float flexGrow = 0.f;
  /// Defaults to 0 (unlike CSS `flex-shrink: 1`) so layout does not shrink views unless opted in.
  float flexShrink = 0.f;
  float minSize = 0.f;
  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
};

} // namespace flux
