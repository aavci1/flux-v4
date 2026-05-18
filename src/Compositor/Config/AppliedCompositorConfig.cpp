#include "Compositor/Config/AppliedCompositorConfig.hpp"

#include <cstdio>

namespace flux::compositor {

AppliedCompositorConfig applyCompositorConfig(CompositorConfig const& config, Canvas& canvas) {
  AppliedCompositorConfig applied{
      .config = config,
      .backgroundFill = config.backgroundGradientEnd
                            ? FillStyle::linearGradient(config.backgroundColor,
                                                        *config.backgroundGradientEnd,
                                                        {0.f, 0.f},
                                                        {1.f, 1.f})
                            : FillStyle::solid(config.backgroundColor),
      .wallpaperImage = nullptr,
  };
  if (config.wallpaperPath) {
    applied.wallpaperImage = loadImageFromFile(*config.wallpaperPath, canvas.gpuDevice());
    if (!applied.wallpaperImage) {
      std::fprintf(stderr, "flux-compositor: failed to load wallpaper %s\n", config.wallpaperPath->c_str());
    }
  }
  return applied;
}

void drawCompositorBackground(Canvas& canvas,
                              AppliedCompositorConfig const& config,
                              std::uint32_t outputWidth,
                              std::uint32_t outputHeight) {
  canvas.clear(config.config.backgroundColor);
  auto const bounds = Rect::sharp(0.f, 0.f, static_cast<float>(outputWidth), static_cast<float>(outputHeight));
  if (config.config.backgroundGradientEnd) {
    canvas.drawRect(bounds,
                    CornerRadius{0.f},
                    config.backgroundFill,
                    StrokeStyle::none(),
                    ShadowStyle::none());
  }
  if (config.wallpaperImage) {
    canvas.drawImage(*config.wallpaperImage, bounds, config.config.wallpaperMode);
  }
}

} // namespace flux::compositor
