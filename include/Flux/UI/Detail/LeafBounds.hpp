#pragma once

/// \file Flux/UI/Detail/LeafBounds.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

namespace flux::detail {

/// Fallback when there is no explicit width/height from modifiers: pick \p childFrame if non-zero,
/// else expand from \p constraints.
Rect resolveLeafBounds(Rect const& frame, Rect const& childFrame, LayoutConstraints const& constraints);

float vStackSlotOffsetX(float itemW, float slotW, Alignment a);

/// Resolves final window-space bounds for a leaf from optional explicit size (\p explicitBox from
/// modifiers), the parent-assigned \p childFrame, numeric \p constraints, and stack \p hints.
///
/// When both explicit width and height are positive, applies \ref LayoutHints `hStackCrossAlign` /
/// `vStackCrossAlign` the same way as for an explicitly sized rect in a stack cell (including when the
/// child frame is larger than the explicit box on an axis). Otherwise delegates to \ref
/// resolveLeafBounds (e.g. \c Text / \c Image with no explicit box, or partial explicit size).
Rect resolveLeafLayoutBounds(Rect const& explicitBox, Rect const& childFrame,
                             LayoutConstraints const& constraints, LayoutHints const& hints);

} // namespace flux::detail
