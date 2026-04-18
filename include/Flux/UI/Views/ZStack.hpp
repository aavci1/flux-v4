#pragma once

/// \file Flux/UI/Views/ZStack.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Alignment.hpp>
#include <Flux/UI/Element.hpp>

#include <vector>

namespace flux {

/// Overlays children in one stack. There is no padding field on `ZStack`: inset via **`.padding()`** on
/// the wrapping `Element`, another stack, or the outer frame in the tree if you need margins.
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
/// full proposed size, then \ref ZStack::horizontalAlignment / \ref ZStack::verticalAlignment offset each child's frame using its intrinsic size
/// (or the expanded frame when `flexGrow > 0`) within that box.
///
/// **Overlay composition:** siblings that share one coordinate system (e.g. a track `Rectangle` and a
/// thumb `Rectangle` with `frame` positions relative to each other) should use `Start` / `Start`
/// so every layer keeps the same origin. Set the alignments to `Center` when you want to center
/// independent children (e.g. a label over a full-bleed background).
///
/// To clip children to the stack’s bounds (e.g. scroll viewport), chain **`.clipContent(true)`** on the
/// `Element` that wraps this `ZStack` (same as other views).
struct ZStack : ViewModifiers<ZStack> {
  void layout(LayoutContext&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  Alignment horizontalAlignment = Alignment::Start;
  Alignment verticalAlignment = Alignment::Start;
  std::vector<Element> children;
};

} // namespace flux
