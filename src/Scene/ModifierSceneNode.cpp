#include <Flux/Scene/ModifierSceneNode.hpp>

#include <Flux/Scene/Renderer.hpp>

#include <algorithm>

namespace flux {

namespace {

Rect intersectRects(Rect lhs, Rect rhs) {
  float const x0 = std::max(lhs.x, rhs.x);
  float const y0 = std::max(lhs.y, rhs.y);
  float const x1 = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
  float const y1 = std::min(lhs.y + lhs.height, rhs.y + rhs.height);
  if (x1 <= x0 || y1 <= y0) {
    return {};
  }
  return Rect{x0, y0, x1 - x0, y1 - y0};
}

Rect expandForStrokeAndShadow(Rect rect, StrokeStyle const& stroke, ShadowStyle const& shadow) {
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

} // namespace

ModifierSceneNode::ModifierSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Modifier, id) {}

bool ModifierSceneNode::paints() const noexcept {
  return !fill.isNone() || !stroke.isNone() || !shadow.isNone();
}

void ModifierSceneNode::applyNodeState(Renderer& renderer) const {
  if (clip.has_value()) {
    renderer.clipRect(*clip);
  }
  renderer.setOpacity(opacity);
  renderer.setBlendMode(blendMode);
}

void ModifierSceneNode::rebuildLocalPaint() {
  SceneNode::rebuildLocalPaint();
  if (!children().empty() && paints()) {
    Rect childBounds = children().front()->bounds;
    localPaintCache().push_back(DrawRectPaintCommand{
        .rect = childBounds,
        .cornerRadius = cornerRadius,
        .fill = fill,
        .stroke = stroke,
        .shadow = shadow,
    });
  }
  markBoundsDirty();
}

Rect ModifierSceneNode::computeOwnBounds() const {
  if (children().empty() || !paints()) {
    return {};
  }
  return expandForStrokeAndShadow(children().front()->bounds, stroke, shadow);
}

Rect ModifierSceneNode::adjustSubtreeBounds(Rect r) const {
  if (clip.has_value()) {
    return intersectRects(r, *clip);
  }
  return r;
}

SceneNode* ModifierSceneNode::hitTest(Point local) {
  if (clip.has_value() && !clip->contains(local)) {
    return nullptr;
  }
  return SceneNode::hitTest(local);
}

SceneNode const* ModifierSceneNode::hitTest(Point local) const {
  return const_cast<ModifierSceneNode*>(this)->hitTest(local);
}

} // namespace flux
