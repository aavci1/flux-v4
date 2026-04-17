#include <Flux/Scene/HitTester.hpp>

#include <Flux/Scene/CustomTransformSceneNode.hpp>
#include <Flux/Scene/SceneTree.hpp>

namespace flux {

namespace {

Point pointInChildSpace(SceneNode const& parent, SceneNode const& child, Point point) {
  if (auto const* transformNode = dynamic_cast<CustomTransformSceneNode const*>(&parent)) {
    return transformNode->transform.inverse().apply(point) - child.position;
  }
  return point - child.position;
}

std::optional<HitResult> hitTestSceneTreeNode(SceneNode const& node, Point point,
                                              std::function<bool(NodeId)> const* acceptTarget) {
  if (!node.bounds.contains(point)) {
    return std::nullopt;
  }
  for (auto it = node.children().rbegin(); it != node.children().rend(); ++it) {
    Point const childPoint = pointInChildSpace(node, **it, point);
    if (auto hit = hitTestSceneTreeNode(**it, childPoint, acceptTarget)) {
      return hit;
    }
  }
  if (node.paints() || node.interaction()) {
    if (!acceptTarget || (*acceptTarget)(node.id())) {
      return HitResult{.nodeId = node.id(), .localPoint = point};
    }
  }
  return std::nullopt;
}

std::optional<Point> localPointForSceneTreeNode(SceneNode const& node, Point point, NodeId targetId) {
  if (node.id() == targetId) {
    return point;
  }
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    if (auto local = localPointForSceneTreeNode(*child, pointInChildSpace(node, *child, point), targetId)) {
      return local;
    }
  }
  return std::nullopt;
}

} // namespace

std::optional<HitResult> HitTester::hitTest(SceneTree const& tree, Point rootPoint) const {
  return hitTest(tree, rootPoint, [](NodeId) { return true; });
}

std::optional<HitResult> HitTester::hitTest(SceneTree const& tree, Point rootPoint,
                                            std::function<bool(NodeId)> const& acceptTarget) const {
  return hitTestSceneTreeNode(tree.root(), rootPoint, &acceptTarget);
}

std::optional<Point> HitTester::localPointForNode(SceneTree const& tree, Point rootPoint, NodeId targetId) const {
  return localPointForSceneTreeNode(tree.root(), rootPoint, targetId);
}

} // namespace flux
