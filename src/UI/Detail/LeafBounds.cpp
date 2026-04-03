#include <Flux/UI/Detail/LeafBounds.hpp>

#include <cmath>

namespace flux::detail {

Rect resolveLeafBounds(Rect const& frame, Rect const& childFrame, LayoutConstraints const& constraints) {
  Rect bounds = frame;
  if (childFrame.width > 0.f || childFrame.height > 0.f) {
    bounds = childFrame;
  }
  if (bounds.width <= 0.f || bounds.height <= 0.f) {
    float const mxW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
    float const mxH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
    if (bounds.width <= 0.f && mxW > 0.f) {
      bounds.width = mxW;
    }
    if (bounds.height <= 0.f && mxH > 0.f) {
      bounds.height = mxH;
    }
  }
  return bounds;
}

float vStackSlotOffsetX(float itemW, float slotW, HorizontalAlignment a) {
  switch (a) {
  case HorizontalAlignment::Leading:
    return 0.f;
  case HorizontalAlignment::Center:
    return (slotW - itemW) * 0.5f;
  case HorizontalAlignment::Trailing:
    return slotW - itemW;
  }
  return 0.f;
}

Rect resolveLeafLayoutBounds(Rect const& explicitBox, Rect const& childFrame,
                             LayoutConstraints const& constraints, LayoutHints const& hints) {
  if (explicitBox.width <= 0.f || explicitBox.height <= 0.f) {
    return resolveLeafBounds(explicitBox, childFrame, constraints);
  }
  if (childFrame.width <= 0.f && childFrame.height <= 0.f) {
    return resolveLeafBounds(explicitBox, childFrame, constraints);
  }
  // Explicit width and height from modifiers: align within stack-assigned childFrame using hints.
  if (hints.hStackCrossAlign && childFrame.height > explicitBox.height + 1e-4f) {
    float const x = childFrame.x;
    float const w = childFrame.width;
    float y = childFrame.y;
    switch (*hints.hStackCrossAlign) {
    case VerticalAlignment::Top:
    case VerticalAlignment::FirstBaseline:
      y = childFrame.y;
      break;
    case VerticalAlignment::Center:
      y = childFrame.y + (childFrame.height - explicitBox.height) * 0.5f;
      break;
    case VerticalAlignment::Bottom:
      y = childFrame.y + childFrame.height - explicitBox.height;
      break;
    }
    return Rect{x, y, w, explicitBox.height};
  }
  // When vStackCrossAlign is set, center (or lead/trail) the explicit box horizontally in the slot.
  if (hints.vStackCrossAlign) {
    float const dx = vStackSlotOffsetX(explicitBox.width, childFrame.width, *hints.vStackCrossAlign);
    return Rect{childFrame.x + dx, childFrame.y, explicitBox.width, explicitBox.height};
  }
  // Explicit size from modifiers; layout-space offset is applied in Element::buildWithModifiers.
  return Rect{childFrame.x, childFrame.y, explicitBox.width, explicitBox.height};
}

} // namespace flux::detail
