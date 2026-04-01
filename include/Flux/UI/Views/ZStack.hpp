#pragma once

/// \file Flux/UI/Views/ZStack.hpp
///
/// Part of the Flux public API.


#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>

#include <vector>

namespace flux {

/// Overlays children in one stack. There is no padding field on `ZStack` (unlike `Grid`):
/// inset the whole stack with a parent that applies padding (e.g. wrap in another stack or pad the
/// outer frame in the tree) if you need margins inside the window.
///
/// Layout uses the parent’s proposed size when finite; each child is measured against that box, then
/// the stack’s reported size is `max` of that inner box and the largest child on each axis (after the
/// same fallback when an axis is unknown). That matches `build`, where each child frame uses
/// `max(intrinsic, inner)` so ancestors see the true footprint (e.g. scrollable content taller than
/// the viewport proposal).
///
/// During `build`, the shared inner width/height uses the same `max(proposed, largest child)` rule
/// as `measure` before laying out children. Each child receives that box; child frames are expanded
/// with `max(intrinsic, inner)` on each axis so nested flex (e.g. `HStack` + `Spacer`) still sees the
/// full proposed size, then `hAlign` / `vAlign` offset each child's frame using its intrinsic size
/// (or the expanded frame when `flexGrow > 0`) within that box.
///
/// **Overlay composition:** siblings that share one coordinate system (e.g. a track `Rectangle` and a
/// thumb `Rectangle` with `frame` positions relative to each other) should use `Leading` and `Top`
/// so every layer keeps the same origin. Default `Center` is for centering independent children
/// (e.g. label over a full-bleed background).
struct ZStack {
  HorizontalAlignment hAlign = HorizontalAlignment::Center;
  VerticalAlignment vAlign = VerticalAlignment::Center;
  bool clip = false;
  std::vector<Element> children;
};

} // namespace flux
