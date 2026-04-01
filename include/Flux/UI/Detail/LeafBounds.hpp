#pragma once

/// \file Flux/UI/Detail/LeafBounds.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

namespace flux::detail {

Rect resolveLeafBounds(Rect const& frame, Rect const& childFrame, LayoutConstraints const& constraints);

float vStackSlotOffsetX(float itemW, float slotW, HorizontalAlignment a);

/// `Rectangle` with explicit `frame` size: stretch to cell width; apply \ref LayoutHints
/// `hStackCrossAlign` / `vStackCrossAlign` when the laid-out child frame is larger than the explicit box (HStack / VStack
/// cells). When the child matches the explicit frame (full cell), alignment is a no-op. Comparisons
/// use `1e-4f` tolerance for float coordinates. `Text` does not use this helper — it aligns runs via
/// constraints in `Element::Model<Text>`.
Rect resolveRectangleBounds(Rect const& frame, Rect const& childFrame, LayoutConstraints const& constraints,
                            LayoutHints const& hints);

} // namespace flux::detail
