#pragma once

/// \file Flux/SceneGraph/SceneInteraction.hpp
///
/// Interaction and focus traversal helpers for the pure scenegraph.

#include <Flux/SceneGraph/InteractionData.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace flux::scenegraph {

class SceneGraph;
class SceneNode;

std::pair<SceneNode const*, InteractionData const*> findInteractionByKey(SceneGraph const& graph,
                                                                         ComponentKey const& key);
std::pair<SceneNode const*, InteractionData const*> findClosestInteractionByKey(SceneGraph const& graph,
                                                                                ComponentKey const& key);

std::optional<InteractionHitResult> hitTestInteraction(SceneGraph const& graph, Point rootPoint);
std::optional<InteractionHitResult> hitTestInteraction(
    SceneGraph const& graph, Point rootPoint,
    Reactive::SmallFn<bool(InteractionData const&)> const& acceptTarget);

std::vector<ComponentKey> collectFocusableKeys(SceneGraph const& graph);

} // namespace flux::scenegraph
