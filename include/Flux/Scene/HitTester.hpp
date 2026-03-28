#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

#include <optional>

namespace flux {

class SceneGraph;

struct HitResult {
  NodeId nodeId{};
  Point localPoint{};
};

class HitTester {
public:
  std::optional<HitResult> hitTest(SceneGraph const& graph, Point windowPoint) const;

private:
  std::optional<HitResult> hitTestNode(NodeId id, SceneGraph const& graph, Point point,
                                       Mat3 const& parentTransform) const;
};

} // namespace flux
