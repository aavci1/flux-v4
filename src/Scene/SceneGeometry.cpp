#include "Scene/SceneGeometry.hpp"

#include <algorithm>

namespace flux::scene {

bool rectEmpty(Rect const& rect) noexcept {
  return rect.width == 0.f && rect.height == 0.f;
}

Rect unionRect(Rect lhs, Rect rhs) noexcept {
  if (rectEmpty(lhs)) {
    return rhs;
  }
  if (rectEmpty(rhs)) {
    return lhs;
  }
  float const x0 = std::min(lhs.x, rhs.x);
  float const y0 = std::min(lhs.y, rhs.y);
  float const x1 = std::max(lhs.x + lhs.width, rhs.x + rhs.width);
  float const y1 = std::max(lhs.y + lhs.height, rhs.y + rhs.height);
  return Rect{x0, y0, x1 - x0, y1 - y0};
}

Rect offsetRect(Rect rect, Point delta) noexcept {
  rect.x += delta.x;
  rect.y += delta.y;
  return rect;
}

Rect intersectRects(Rect lhs, Rect rhs) noexcept {
  float const x0 = std::max(lhs.x, rhs.x);
  float const y0 = std::max(lhs.y, rhs.y);
  float const x1 = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
  float const y1 = std::min(lhs.y + lhs.height, rhs.y + rhs.height);
  if (x1 <= x0 || y1 <= y0) {
    return {};
  }
  return Rect{x0, y0, x1 - x0, y1 - y0};
}

Rect expandForStrokeAndShadow(Rect rect, StrokeStyle const& stroke, ShadowStyle const& shadow) noexcept {
  float const strokeInset = stroke.isNone() ? 0.f : stroke.width * 0.5f;
  float const blur = shadow.isNone() ? 0.f : shadow.radius;
  float const left = std::max(strokeInset, blur - shadow.offset.x);
  float const right = std::max(strokeInset, blur + shadow.offset.x);
  float const top = std::max(strokeInset, blur - shadow.offset.y);
  float const bottom = std::max(strokeInset, blur + shadow.offset.y);
  rect.x -= left;
  rect.y -= top;
  rect.width += left + right;
  rect.height += top + bottom;
  return rect;
}

Rect transformBounds(Mat3 const& transform, Rect const& rect) noexcept {
  Point const p0 = transform.apply(Point{rect.x, rect.y});
  Point const p1 = transform.apply(Point{rect.x + rect.width, rect.y});
  Point const p2 = transform.apply(Point{rect.x, rect.y + rect.height});
  Point const p3 = transform.apply(Point{rect.x + rect.width, rect.y + rect.height});
  float const minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
  float const minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
  float const maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
  float const maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
  return Rect{minX, minY, maxX - minX, maxY - minY};
}

} // namespace flux::scene
