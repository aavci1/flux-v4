#pragma once

#include <cstdint>

namespace flux {

enum class HorizontalAlignment : std::uint8_t { Leading, Center, Trailing };

/// `FirstBaseline` uses `firstBaselineOffset`: distance from the box top to the desired first baseline.
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

  /// Distance from the box top to the desired first baseline (only for `VerticalAlignment::FirstBaseline`).
  /// With offset 0, the first baseline is placed on the box top edge (ascenders may draw above the box).
  float firstBaselineOffset = 0.f;
};

} // namespace flux
