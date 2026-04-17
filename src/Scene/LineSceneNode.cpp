#include <Flux/Scene/LineSceneNode.hpp>

#include <algorithm>

namespace flux {

LineSceneNode::LineSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Line, id) {}

void LineSceneNode::rebuildLocalPaint() {
  localPaintCache().clear();
  localPaintCache().push_back(DrawLinePaintCommand{
      .from = from,
      .to = to,
      .stroke = stroke,
  });
  SceneNode::rebuildLocalPaint();
}

Rect LineSceneNode::computeOwnBounds() const {
  float const x0 = std::min(from.x, to.x);
  float const y0 = std::min(from.y, to.y);
  float const x1 = std::max(from.x, to.x);
  float const y1 = std::max(from.y, to.y);
  float const inset = stroke.isNone() ? 0.f : stroke.width * 0.5f;
  return Rect{x0 - inset, y0 - inset, (x1 - x0) + inset * 2.f, (y1 - y0) + inset * 2.f};
}

} // namespace flux
