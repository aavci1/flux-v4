#include <Flux/Scene/PathSceneNode.hpp>

namespace flux {

PathSceneNode::PathSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Path, id) {}

void PathSceneNode::rebuildLocalPaint() {
  localPaintCache().clear();
  localPaintCache().push_back(DrawPathPaintCommand{
      .path = path,
      .fill = fill,
      .stroke = stroke,
      .shadow = shadow,
  });
  SceneNode::rebuildLocalPaint();
}

Rect PathSceneNode::computeOwnBounds() const {
  return path.getBounds();
}

} // namespace flux
