#pragma once

#include <Flux/Scene/CustomTransformSceneNode.hpp>
#include <Flux/Scene/SceneNode.hpp>

#include <functional>
#include <optional>

namespace flux::scene {

inline Point pointInChildSpace(SceneNode const& parent, SceneNode const& child, Point point) {
  if (auto const* transformNode = dynamic_cast<CustomTransformSceneNode const*>(&parent)) {
    return transformNode->transform.inverse().apply(point) - child.position;
  }
  return point - child.position;
}

template<typename Visitor>
void walkSceneTree(SceneNode const& node, Visitor&& visitor) {
  visitor(node);
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    walkSceneTree(*child, visitor);
  }
}

template<typename AcceptTarget>
std::optional<std::pair<SceneNode const*, Point>> hitTestNode(SceneNode const& node, Point point,
                                                              AcceptTarget&& acceptTarget) {
  if (!node.bounds.contains(point)) {
    return std::nullopt;
  }
  for (auto it = node.children().rbegin(); it != node.children().rend(); ++it) {
    Point const childPoint = pointInChildSpace(node, **it, point);
    if (auto hit = hitTestNode(**it, childPoint, acceptTarget)) {
      return hit;
    }
  }
  if (acceptTarget(node)) {
    return std::pair<SceneNode const*, Point>{&node, point};
  }
  return std::nullopt;
}

inline std::optional<Point> localPointForNode(SceneNode const& node, Point point, NodeId targetId) {
  if (node.id() == targetId) {
    return point;
  }
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    if (auto local = localPointForNode(*child, pointInChildSpace(node, *child, point), targetId)) {
      return local;
    }
  }
  return std::nullopt;
}

} // namespace flux::scene
