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

struct HitResult {
  NodeId nodeId{};
  Point localPoint{};
};

class HitTester {
public:
  std::optional<HitResult> hitTest(SceneTree const& tree, Point rootPoint) const;
  std::optional<HitResult> hitTest(SceneTree const& tree, Point rootPoint,
                                   std::function<bool(NodeId)> const& acceptTarget) const;
  std::optional<Point> localPointForNode(SceneTree const& tree, Point rootPoint, NodeId targetId) const;
};

} // namespace flux
