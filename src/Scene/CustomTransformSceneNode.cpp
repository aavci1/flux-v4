#include <Flux/Scene/CustomTransformSceneNode.hpp>

#include <Flux/Scene/Renderer.hpp>

#include "Scene/SceneGeometry.hpp"

namespace flux {

CustomTransformSceneNode::CustomTransformSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Custom, id) {}

void CustomTransformSceneNode::applyNodeState(Renderer& renderer) const {
  renderer.transform(transform);
}

void CustomTransformSceneNode::recomputeBounds() {
  Rect subtree{};
  for (std::unique_ptr<SceneNode> const& child : children()) {
    subtree = scene::unionRect(subtree, scene::transformBounds(transform, scene::offsetRect(child->bounds, child->position)));
  }
  if (subtree == bounds) {
    clearBoundsDirty();
    return;
  }
  bounds = subtree;
  clearBoundsDirty();
  if (parent()) {
    parent()->recomputeBounds();
  }
}

SceneNode* CustomTransformSceneNode::hitTest(Point local) {
  if (children().empty()) {
    return nullptr;
  }
  Point const childLocal = transform.inverse().apply(local);
  return children().front()->hitTest(childLocal);
}

SceneNode const* CustomTransformSceneNode::hitTest(Point local) const {
  return const_cast<CustomTransformSceneNode*>(this)->hitTest(local);
}

} // namespace flux
