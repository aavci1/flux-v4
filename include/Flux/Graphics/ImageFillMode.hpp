#pragma once

#include <cstdint>

namespace flux {

enum class ImageFillMode : std::uint8_t {
  Stretch,
  Fit,
  Cover,
  Center,
  Tile,
};

} // namespace flux
