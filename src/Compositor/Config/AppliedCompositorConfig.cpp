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

} // namespace flux::compositor
