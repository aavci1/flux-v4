#pragma once

/// \file Flux/UI/Views/ScrollView.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/OffsetView.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <optional>
#include <vector>

namespace flux {

/// Clamps \p o so the scrolled content does not overscroll past the viewport for \p axis.
/// Non-scrolling axes are zeroed (horizontal-only keeps `o.y == 0`, vertical-only keeps `o.x == 0`).
Point clampScrollOffset(ScrollAxis axis, Point o, Size const& viewport, Size const& content);

/// Scrollable region: children are laid out in an \ref OffsetView and can be dragged or wheel-scrolled.
struct ScrollView : ViewModifiers<ScrollView> {
  // ── Layout / axis ─────────────────────────────────────────────────────────

  ScrollAxis axis = ScrollAxis::Vertical;
  float flexGrow = 1.f;
  float flexShrink = 0.f;
  float minSize = 0.f;
  std::vector<Element> children;

  // ── Component protocol ─────────────────────────────────────────────────────

  Element body() const;
};

} // namespace flux
