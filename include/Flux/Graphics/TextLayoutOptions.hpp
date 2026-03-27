#pragma once

#include <cstdint>

namespace flux {

enum class HorizontalAlignment : std::uint8_t { Leading, Center, Trailing };

enum class VerticalAlignment : std::uint8_t { Top, Center, Bottom, FirstBaseline };

enum class TextWrapping : std::uint8_t {
  NoWrap,       ///< Single line; ignores box width for wrapping.
  Wrap,         ///< Break at word boundaries (default).
  WrapAnywhere, ///< Break at any character when no word boundary fits.
};

struct TextLayoutOptions {
  HorizontalAlignment horizontalAlignment = HorizontalAlignment::Leading;
  VerticalAlignment verticalAlignment = VerticalAlignment::Top;
  TextWrapping wrapping = TextWrapping::Wrap;
  float lineHeight = 0.f; ///< 0 = font natural line height.
  int maxLines = 0;       ///< 0 = unlimited.
};

} // namespace flux
