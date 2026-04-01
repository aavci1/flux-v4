#pragma once

/// \file Flux/Graphics/ImageFillMode.hpp
///
/// Part of the Flux public API.


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
