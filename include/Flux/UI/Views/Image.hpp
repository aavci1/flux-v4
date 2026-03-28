#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <functional>
#include <memory>

namespace flux::views {

/// Image view component. `source` references `flux::Image` (bitmap); distinct from this `Image` view type.
struct Image {
  std::shared_ptr<flux::Image> source;
  Rect frame{};
  ImageFillMode fillMode = ImageFillMode::Cover;
  CornerRadius cornerRadius{};
  float opacity = 1.f;
  std::function<void()> onTap;
};

} // namespace flux::views
