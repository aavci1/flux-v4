#pragma once

/// \file Flux/Scene/HitTester.hpp
///
/// Part of the Flux public API.


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
  /// transparent so input reaches geometry behind them (e.g. overlay text with \ref Cursor::Inherit
  /// over a control that sets \ref Cursor::Hand).
  std::optional<HitResult> hitTest(SceneGraph const& graph, Point windowPoint,
                                   std::function<bool(NodeId)> const& acceptTarget) const;

  /// Pointer in the same local space as \ref HitResult::localPoint for \p targetId, if that node
  /// exists in the scene graph (used to deliver drags to the press target regardless of frontmost hit).
  std::optional<Point> localPointForNode(SceneGraph const& graph, Point windowPoint,
                                         NodeId targetId) const;

private:
  std::optional<HitResult> hitTestNode(NodeId id, SceneGraph const& graph, Point point,
                                       Mat3 const& parentTransform,
                                       std::function<bool(NodeId)> const* acceptTarget) const;

  std::optional<Point> localPointForNodeImpl(NodeId id, SceneGraph const& graph, Point windowPoint,
                                             Mat3 const& parentTransform, NodeId targetId) const;
};

} // namespace flux
