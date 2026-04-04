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

float vStackSlotOffsetX(float itemW, float slotW, Alignment a) {
  switch (a) {
  case Alignment::Start:
  case Alignment::Stretch:
    return 0.f;
  case Alignment::Center:
    return (slotW - itemW) * 0.5f;
  case Alignment::End:
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
    case Alignment::Start:
    case Alignment::Stretch:
      y = childFrame.y;
      break;
    case Alignment::Center:
      y = childFrame.y + (childFrame.height - explicitBox.height) * 0.5f;
      break;
    case Alignment::End:
      y = childFrame.y + childFrame.height - explicitBox.height;
      break;
    }
    float const outH = *hints.hStackCrossAlign == Alignment::Stretch ? childFrame.height
                                                                     : explicitBox.height;
    return Rect{x, y, w, outH};
  }
  // When vStackCrossAlign is set, center (or lead/trail) the explicit box horizontally in the slot.
  if (hints.vStackCrossAlign) {
    if (*hints.vStackCrossAlign == Alignment::Stretch) {
      return Rect{childFrame.x, childFrame.y, childFrame.width, explicitBox.height};
    }
    float const dx = vStackSlotOffsetX(explicitBox.width, childFrame.width, *hints.vStackCrossAlign);
    return Rect{childFrame.x + dx, childFrame.y, explicitBox.width, explicitBox.height};
  }
  // Explicit size from modifiers; layout-space offset is applied in Element::buildWithModifiers.
  return Rect{childFrame.x, childFrame.y, explicitBox.width, explicitBox.height};
}

} // namespace flux::detail
