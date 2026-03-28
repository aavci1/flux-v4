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
  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
};

} // namespace flux
