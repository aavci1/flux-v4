#pragma once

#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>

#include <cstddef>
#include <vector>

namespace flux {

/// Fixed-column grid: children flow left-to-right, top-to-bottom (row-major).
///
/// When the available width is unknown (`innerWidth == 0`), `cellW` is zero and each child is
/// measured with unbounded width. Views that expand to fill width (e.g. `Rectangle` with no
/// frame) may measure at zero width — the grid reports `{0, totalHeight}` until a parent assigns a
/// finite width, same idea as `VStack`.
struct Grid {
  /// Number of columns. Values below 1 are clamped to 1 during layout.
  std::size_t columns = 2;
  /// Gap between columns (horizontal) and between rows (vertical).
  float hSpacing = 8.f;
  float vSpacing = 8.f;
  /// Inset on all four sides.
  float padding = 0.f;
  /// Alignment of each child within its cell when the child is narrower or shorter than the cell.
  HorizontalAlignment hAlign = HorizontalAlignment::Leading;
  VerticalAlignment vAlign = VerticalAlignment::Top;
  /// Children in row-major order (left-to-right, top-to-bottom).
  std::vector<Element> children;
};

} // namespace flux
