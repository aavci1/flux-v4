#include <Flux/Scene/HitTester.hpp>

#include <Flux/Scene/SceneTree.hpp>

#include "Scene/SceneTraversal.hpp"

namespace flux {

std::optional<HitResult> HitTester::hitTest(SceneTree const& tree, Point rootPoint) const {
  return hitTest(tree, rootPoint, [](NodeId) { return true; });
}

std::optional<HitResult> HitTester::hitTest(SceneTree const& tree, Point rootPoint,
                                            std::function<bool(NodeId)> const& acceptTarget) const {
  if (auto hit = scene::hitTestNode(tree.root(), rootPoint, [&](SceneNode const& node) {
        return (node.paints() || node.interaction()) && acceptTarget(node.id());
      })) {
    return HitResult{.nodeId = hit->first->id(), .localPoint = hit->second};
  }
  return std::nullopt;
}

std::optional<Point> HitTester::localPointForNode(SceneTree const& tree, Point rootPoint, NodeId targetId) const {
  return scene::localPointForNode(tree.root(), rootPoint, targetId);
}

} // namespace flux
