#include <Flux/Scene/RectSceneNode.hpp>

#include "Scene/SceneGeometry.hpp"

namespace flux {

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
  return scene::expandForStrokeAndShadow(Rect{0.f, 0.f, size.width, size.height}, stroke, shadow);
}

} // namespace flux
