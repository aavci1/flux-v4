#include "Compositor/Wayland/LayerShellZones.hpp"

#include <doctest/doctest.h>

#include <vector>

TEST_CASE("layer shell reserved zones aggregate top bar and dock") {
  std::vector<flux::compositor::LayerShellReservedZoneInput> layers{
      {.nameSpace = "lambda.topbar", .exclusiveZone = 36},
      {.nameSpace = "lambda.dock", .exclusiveZone = 0, .anchor = 4, .marginBottom = 8, .extent = 64},
  };
  auto const zones = flux::compositor::aggregateLayerShellReservedZones(layers);
  CHECK(zones.topBar == 36);
  CHECK(zones.dock == 72);
}

TEST_CASE("layer shell reserved zones ignore unrelated namespaces") {
  std::vector<flux::compositor::LayerShellReservedZoneInput> layers{
      {.nameSpace = "com.example.panel", .exclusiveZone = 48},
  };
  auto const zones = flux::compositor::aggregateLayerShellReservedZones(layers);
  CHECK(zones.topBar == 0);
  CHECK(zones.dock == 0);
}
