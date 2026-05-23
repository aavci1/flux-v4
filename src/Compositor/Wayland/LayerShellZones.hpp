#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace flux::compositor {

struct LayerShellReservedZoneInput {
  char const* nameSpace = nullptr;
  std::int32_t exclusiveZone = 0;
  std::uint32_t anchor = 0;
  std::int32_t marginBottom = 0;
  std::int32_t extent = 0;
};

struct LayerShellReservedZones {
  std::int32_t topBar = 0;
  std::int32_t dock = 0;
};

[[nodiscard]] LayerShellReservedZones aggregateLayerShellReservedZones(
    std::span<LayerShellReservedZoneInput const> layers);

} // namespace flux::compositor
