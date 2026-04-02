#pragma once

/// \file Flux/UI/Views/Grid.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <cstddef>
#include <vector>

namespace flux {

/// Fixed-column grid: children flow left-to-right, top-to-bottom (row-major).
///
/// When the available width is unknown (`innerWidth == 0`), `cellW` is zero and each child is
/// measured with unbounded width. Views that expand to fill width (e.g. `Rectangle` with no
/// frame) may measure at zero width — the grid reports `{0, totalHeight}` until a parent assigns a
/// finite width, same idea as `VStack`.
///
/// `Spacer` has no flex axis here (unlike `VStack` / `HStack`), but it remains a valid child: it is
/// measured like other cells, occupies a full column in row-major order, and produces no output.
/// Slot / key indices stay aligned by advancing the child cursor without calling `Spacer::build`.
///
/// For outer inset or clipping, use **`.padding(float)`** / **`.clipContent(bool)`** on the wrapping
/// `Element`.
struct Grid : ViewModifiers<Grid> {
  void build(BuildContext&) const;
  Size measure(BuildContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  /// Number of columns. Values below 1 are clamped to 1 during layout.
  std::size_t columns = 2;
  /// Gap between columns (horizontal) and between rows (vertical).
  float hSpacing = 8.f;
  float vSpacing = 8.f;
  /// Alignment of each child within its cell when the child is narrower or shorter than the cell.
  HorizontalAlignment hAlign = HorizontalAlignment::Leading;
  VerticalAlignment vAlign = VerticalAlignment::Top;
  /// Children in row-major order (left-to-right, top-to-bottom).
  std::vector<Element> children;
};

} // namespace flux
