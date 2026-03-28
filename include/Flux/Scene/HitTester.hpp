#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

#include <functional>
#include <optional>

namespace flux {

class SceneGraph;

struct HitResult {
  NodeId nodeId{};
  Point localPoint{};
};

class HitTester {
public:
  /// Pure geometry: first front-to-back hit (e.g. scene graph debugging).
  std::optional<HitResult> hitTest(SceneGraph const& graph, Point windowPoint) const;

  /// If \p acceptTarget is set, only nodes for which it returns true may be hit. Other nodes are
  /// transparent so input reaches geometry behind them (e.g. text without handlers over a button).
  std::optional<HitResult> hitTest(SceneGraph const& graph, Point windowPoint,
                                   std::function<bool(NodeId)> const& acceptTarget) const;

private:
  std::optional<HitResult> hitTestNode(NodeId id, SceneGraph const& graph, Point point,
                                       Mat3 const& parentTransform,
                                       std::function<bool(NodeId)> const* acceptTarget) const;
};

} // namespace flux
