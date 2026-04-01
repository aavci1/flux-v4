#pragma once

/// \file Flux/UI/Views/Image.hpp
///
/// Part of the Flux public API.


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
  float offsetX = 0.f;
  float offsetY = 0.f;
  float width = 0.f;
  float height = 0.f;
  ImageFillMode fillMode = ImageFillMode::Cover;
  CornerRadius cornerRadius{};
  float opacity = 1.f;
  std::function<void()> onTap;
};

} // namespace flux::views
