#pragma once

/// \file Flux/Scene/HitTester.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

#include <functional>
#include <optional>

namespace flux {

class SceneTree;
class SceneGraph;

struct HitResult {
  NodeId nodeId{};
  Point localPoint{};
};

class HitTester {
public:
  std::optional<HitResult> hitTest(SceneGraph const& graph, Point windowPoint) const;
  std::optional<HitResult> hitTest(SceneGraph const& graph, Point windowPoint,
                                   std::function<bool(NodeId)> const& acceptTarget) const;
  std::optional<Point> localPointForNode(SceneGraph const& graph, Point windowPoint, NodeId targetId) const;

  std::optional<HitResult> hitTest(SceneTree const& tree, Point rootPoint) const;
  std::optional<HitResult> hitTest(SceneTree const& tree, Point rootPoint,
                                   std::function<bool(NodeId)> const& acceptTarget) const;
  std::optional<Point> localPointForNode(SceneTree const& tree, Point rootPoint, NodeId targetId) const;

private:
  std::optional<HitResult> hitTestNode(NodeId id, SceneGraph const& graph, Point point, Mat3 const& parentTransform,
                                       std::function<bool(NodeId)> const* acceptTarget) const;
  std::optional<Point> localPointForNodeImpl(NodeId id, SceneGraph const& graph, Point windowPoint,
                                             Mat3 const& parentTransform, NodeId targetId) const;
};

} // namespace flux
