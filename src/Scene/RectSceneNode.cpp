#include <Flux/Scene/RectSceneNode.hpp>

namespace flux {

namespace {

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

RectSceneNode::RectSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Rect, id) {}

void RectSceneNode::rebuildLocalPaint() {
  SceneNode::rebuildLocalPaint();
  localPaintCache().push_back(DrawRectPaintCommand{
      .rect = Rect{0.f, 0.f, size.width, size.height},
      .cornerRadius = cornerRadius,
      .fill = fill,
      .stroke = stroke,
      .shadow = shadow,
  });
}

Rect RectSceneNode::computeOwnBounds() const {
  return expandForStrokeAndShadow(Rect{0.f, 0.f, size.width, size.height}, stroke, shadow);
}

} // namespace flux
