#pragma once

#include "Compositor/Config/AppliedCompositorConfig.hpp"
#include "Compositor/Config/CompositorConfig.hpp"
#include "Compositor/Presenter.hpp"
#include "Compositor/WaylandServer.hpp"

#include <Flux/Graphics/Canvas.hpp>

#include <functional>

namespace flux::compositor {

struct CompositorConfigWatchContext {
  LoadedCompositorConfig& loadedConfig;
  AppliedCompositorConfig& appliedConfig;
  WaylandServer& wayland;
  Presenter& presenter;
  flux::Canvas& canvas;
  std::function<CompositorConfig()> effectiveConfig;
  std::function<void(bool forceOutputScale)> applyOutputScale;
};

/// Returns true when config was reloaded and applied.
bool maybeReloadCompositorConfig(CompositorConfigWatchContext& ctx);

void applyCompositorRuntimeConfig(CompositorConfigWatchContext& ctx, bool forceOutputScale = false);

} // namespace flux::compositor
