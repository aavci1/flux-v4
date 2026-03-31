#pragma once

#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>

#include <vector>

namespace flux {

/// Overlays children in one stack. There is no padding field on `ZStack` (unlike `Grid`):
/// inset the whole stack with a parent that applies padding (e.g. wrap in another stack or pad the
/// outer frame in the tree) if you need margins inside the window.
///
/// Layout uses the parent’s proposed size when finite; each child is measured against that box, then
/// the stack’s own size is the max of children’s widths and heights. If the proposed width or height
/// is unknown (`<= 0` after constraints), that axis falls back to the largest measured child so the
/// stack still gets a non-zero footprint.
///
/// During `build`, every child receives the same inner width/height (that shared box). Child frames
/// are expanded with `max(intrinsic, inner)` on each axis so nested flex (e.g. `HStack` + `Spacer`)
/// still sees the full proposed size, then `hAlign` / `vAlign` offset the frame within that box.
struct ZStack {
  HorizontalAlignment hAlign = HorizontalAlignment::Center;
  VerticalAlignment vAlign = VerticalAlignment::Center;
  bool clip = false;
  std::vector<Element> children;
};

} // namespace flux
