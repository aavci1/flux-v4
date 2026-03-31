#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cmath>

namespace flux::detail {

inline Rect resolveLeafBounds(Rect const& frame, Rect const& childFrame,
                              LayoutConstraints const& constraints) {
  Rect bounds = frame;
  if (childFrame.width > 0.f || childFrame.height > 0.f) {
    bounds = childFrame;
  }
  if (bounds.width <= 0.f || bounds.height <= 0.f) {
    float const w = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
    float const h = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
    if (w > 0.f && h > 0.f) {
      bounds = Rect{0, 0, w, h};
    }
  }
  return bounds;
}

inline float vStackSlotOffsetX(float itemW, float slotW, HorizontalAlignment a) {
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

/// `Rectangle` with explicit `frame` size: stretch to cell width, align vertically when the cell is
/// taller than the intrinsic height and `constraints.hStackCrossAlign` is set (HStack row cells).
inline Rect resolveRectangleBounds(Rect const& frame, Rect const& childFrame,
                                   LayoutConstraints const& constraints) {
  if (frame.width <= 0.f || frame.height <= 0.f) {
    return resolveLeafBounds(frame, childFrame, constraints);
  }
  if (childFrame.width <= 0.f && childFrame.height <= 0.f) {
    return resolveLeafBounds(frame, childFrame, constraints);
  }
  if (constraints.hStackCrossAlign && childFrame.height > frame.height + 1e-4f) {
    float const x = childFrame.x;
    float const w = childFrame.width;
    float const h = frame.height;
    float y = childFrame.y;
    switch (*constraints.hStackCrossAlign) {
    case VerticalAlignment::Top:
    case VerticalAlignment::FirstBaseline:
      y = childFrame.y;
      break;
    case VerticalAlignment::Center:
      y = childFrame.y + (childFrame.height - frame.height) * 0.5f;
      break;
    case VerticalAlignment::Bottom:
      y = childFrame.y + childFrame.height - frame.height;
      break;
    }
    return Rect{x, y, w, h};
  }
  if (constraints.vStackCrossAlign && childFrame.width > frame.width + 1e-4f) {
    float const dx = vStackSlotOffsetX(frame.width, childFrame.width, *constraints.vStackCrossAlign);
    return Rect{childFrame.x + dx, childFrame.y, frame.width, frame.height};
  }
  return resolveLeafBounds(frame, childFrame, constraints);
}

} // namespace flux::detail
