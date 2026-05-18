#pragma once

#include "Compositor/Config/CompositorConfig.hpp"

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <cstdint>
#include <memory>

namespace flux::compositor {

struct AppliedCompositorConfig {
  CompositorConfig config;
  FillStyle backgroundFill = FillStyle::solid(Color{0.20f, 0.50f, 0.95f, 1.0f});
  std::shared_ptr<Image> wallpaperImage;
};

[[nodiscard]] AppliedCompositorConfig applyCompositorConfig(CompositorConfig const& config, Canvas& canvas);
void drawCompositorBackground(Canvas& canvas,
                              AppliedCompositorConfig const& config,
                              std::uint32_t outputWidth,
                              std::uint32_t outputHeight);

} // namespace flux::compositor
