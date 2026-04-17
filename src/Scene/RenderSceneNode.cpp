#include <Flux/Scene/RenderSceneNode.hpp>

namespace flux {

RenderSceneNode::RenderSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Render, id) {}

void RenderSceneNode::rebuildLocalPaint() {
  // User draw callbacks remain immediate-mode. The retained tree keeps the callback and its frame;
  // we do not attempt to record arbitrary Canvas calls into PaintCommand here.
  SceneNode::rebuildLocalPaint();
}

Rect RenderSceneNode::computeOwnBounds() const {
  return frame;
}

} // namespace flux
