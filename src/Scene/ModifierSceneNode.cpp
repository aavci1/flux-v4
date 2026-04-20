#include <Flux/Scene/ModifierSceneNode.hpp>

#include <Flux/Scene/Renderer.hpp>

#include "Scene/SceneGeometry.hpp"

namespace flux {

namespace {

void clampRoundRectCornerRadii(float w, float h, CornerRadius& r) {
  if (w <= 0.f || h <= 0.f) {
    return;
  }
  float const maxR = std::min(w, h) * 0.5f;
  r.topLeft = std::min(r.topLeft, maxR);
  r.topRight = std::min(r.topRight, maxR);
  r.bottomRight = std::min(r.bottomRight, maxR);
  r.bottomLeft = std::min(r.bottomLeft, maxR);
  auto fixEdge = [](float& a, float& b, float len) {
    if (a + b > len && len > 0.f) {
      float const s = len / (a + b);
      a *= s;
      b *= s;
    }
  };
  fixEdge(r.topLeft, r.topRight, w);
  fixEdge(r.bottomLeft, r.bottomRight, w);
  fixEdge(r.topLeft, r.bottomLeft, h);
  fixEdge(r.topRight, r.bottomRight, h);
}

bool roundedRectContains(Rect const& rect, CornerRadius cornerRadius, Point p) {
  if (!rect.contains(p)) {
    return false;
  }
  clampRoundRectCornerRadii(rect.width, rect.height, cornerRadius);
  if (cornerRadius.isZero()) {
    return true;
  }

  auto pointInCorner = [&](float left, float top, float radius) {
    if (radius <= 0.f) {
      return true;
    }
    float const cx = left + radius;
    float const cy = top + radius;
    float const dx = p.x - cx;
    float const dy = p.y - cy;
    return dx * dx + dy * dy <= radius * radius;
  };

  float const right = rect.x + rect.width;
  float const bottom = rect.y + rect.height;
  if (p.x < rect.x + cornerRadius.topLeft && p.y < rect.y + cornerRadius.topLeft) {
    return pointInCorner(rect.x, rect.y, cornerRadius.topLeft);
  }
  if (p.x > right - cornerRadius.topRight && p.y < rect.y + cornerRadius.topRight) {
    return pointInCorner(right - cornerRadius.topRight * 2.f, rect.y, cornerRadius.topRight);
  }
  if (p.x > right - cornerRadius.bottomRight && p.y > bottom - cornerRadius.bottomRight) {
    return pointInCorner(right - cornerRadius.bottomRight * 2.f, bottom - cornerRadius.bottomRight * 2.f,
                         cornerRadius.bottomRight);
  }
  if (p.x < rect.x + cornerRadius.bottomLeft && p.y > bottom - cornerRadius.bottomLeft) {
    return pointInCorner(rect.x, bottom - cornerRadius.bottomLeft * 2.f, cornerRadius.bottomLeft);
  }
  return true;
}

} // namespace

ModifierSceneNode::ModifierSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Modifier, id) {}

bool ModifierSceneNode::paints() const noexcept {
  return !fill.isNone() || !stroke.isNone() || !shadow.isNone();
}

void ModifierSceneNode::applyNodeState(Renderer& renderer) const {
  if (clip.has_value()) {
    renderer.clipRect(*clip, cornerRadius);
  }
  renderer.setOpacity(opacity);
  renderer.setBlendMode(blendMode);
}

void ModifierSceneNode::rebuildLocalPaint() {
  SceneNode::rebuildLocalPaint();
  if (paints() && (chromeRect.width > 0.f || chromeRect.height > 0.f)) {
    localPaintCache().push_back(DrawRectPaintCommand{
        .rect = chromeRect,
        .cornerRadius = cornerRadius,
        .fill = fill,
        .stroke = stroke,
        .shadow = shadow,
    });
  }
  markBoundsDirty();
}

Rect ModifierSceneNode::computeOwnBounds() const {
  if (!paints() || (chromeRect.width <= 0.f && chromeRect.height <= 0.f)) {
    return {};
  }
  return scene::expandForStrokeAndShadow(chromeRect, stroke, shadow);
}

Rect ModifierSceneNode::adjustSubtreeBounds(Rect r) const {
  if (clip.has_value()) {
    return scene::intersectRects(r, *clip);
  }
  return r;
}

SceneNode* ModifierSceneNode::hitTest(Point local) {
  if (clip.has_value() && !roundedRectContains(*clip, cornerRadius, local)) {
    return nullptr;
  }
  return SceneNode::hitTest(local);
}

SceneNode const* ModifierSceneNode::hitTest(Point local) const {
  return const_cast<ModifierSceneNode*>(this)->hitTest(local);
}

} // namespace flux
