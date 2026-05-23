#include "Compositor/Wayland/LayerShellZones.hpp"

#include <algorithm>
#include <string_view>

namespace flux::compositor {

namespace {
constexpr std::uint32_t kAnchorBottom = 4u; // ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
}

LayerShellReservedZones aggregateLayerShellReservedZones(
    std::span<LayerShellReservedZoneInput const> layers) {
  LayerShellReservedZones zones;
  for (auto const& layer : layers) {
    if (!layer.nameSpace) continue;
    if (std::string_view(layer.nameSpace) == "lambda.topbar") {
      zones.topBar = std::max(zones.topBar, std::max(0, layer.exclusiveZone));
    } else if (std::string_view(layer.nameSpace) == "lambda.dock" && (layer.anchor & kAnchorBottom) != 0) {
      zones.dock = std::max(zones.dock, layer.extent + std::max(0, layer.marginBottom));
    }
  }
  return zones;
}

} // namespace flux::compositor
