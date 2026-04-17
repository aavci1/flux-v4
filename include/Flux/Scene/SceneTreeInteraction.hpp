#pragma once

/// \file Flux/Scene/SceneTreeInteraction.hpp
///
/// Part of the Flux public API.

#include <Flux/Scene/InteractionData.hpp>
#include <Flux/Scene/SceneTree.hpp>

#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace flux {

struct InteractionHitResult {
  NodeId nodeId{};
  Point localPoint{};
  InteractionData const* interaction = nullptr;
};

std::pair<NodeId, InteractionData const*> findInteractionById(SceneTree const& tree, NodeId id);
std::pair<NodeId, InteractionData const*> findInteractionByKey(SceneTree const& tree, ComponentKey const& key);
std::pair<NodeId, InteractionData const*> findClosestInteractionByKey(SceneTree const& tree,
                                                                      ComponentKey const& key);
std::optional<InteractionHitResult> hitTestInteraction(SceneTree const& tree, Point rootPoint);
std::optional<InteractionHitResult> hitTestInteraction(SceneTree const& tree, Point rootPoint,
                                                       std::function<bool(InteractionData const&)> const& acceptTarget);
std::vector<ComponentKey> collectFocusableKeys(SceneTree const& tree);

} // namespace flux
