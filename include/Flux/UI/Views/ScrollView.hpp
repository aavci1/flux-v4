#pragma once

/// \file Flux/UI/Views/ScrollView.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/OffsetView.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <optional>
#include <vector>

namespace flux {

/// Clamps \p o so the scrolled content does not overscroll past the viewport for \p axis.
/// Non-scrolling axes are zeroed (horizontal-only keeps `o.y == 0`, vertical-only keeps `o.x == 0`).
Point clampScrollOffset(ScrollAxis axis, Point o, Size const &viewport, Size const &content);

/// Scrollable region: children are laid out in an \ref OffsetView and can be dragged or wheel-scrolled.
struct ScrollView : ViewModifiers<ScrollView> {
    // ── Layout / axis ─────────────────────────────────────────────────────────

    ScrollAxis axis = ScrollAxis::Vertical;
    State<Point> scrollOffset {};
    State<Size> viewportSize {};
    State<Size> contentSize {};
    bool dragScrollEnabled = true;
    std::vector<Element> children;

    /// Custom measurement hook used by the measured-component pipeline.
    Size measure(MeasureContext &, LayoutConstraints const &, LayoutHints const &, TextSystem &) const;

    bool operator==(ScrollView const& other) const {
        return axis == other.axis && scrollOffset == other.scrollOffset &&
               viewportSize == other.viewportSize && contentSize == other.contentSize &&
               dragScrollEnabled == other.dragScrollEnabled &&
               elementsStructurallyEqual(children, other.children);
    }

};

} // namespace flux
