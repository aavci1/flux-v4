#pragma once

/// \file Flux/SceneGraph/SceneInteraction.hpp
///
/// Interaction and focus traversal helpers for the pure scenegraph.

#include <Flux/SceneGraph/Interaction.hpp>
#include <Flux/Reactive/SmallFn.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace flux::scenegraph {

class SceneGraph;
class SceneNode;

std::pair<SceneNode const*, Interaction const*> findInteractionByKey(SceneGraph const& graph,
                                                                     ComponentKey const& key);

std::optional<InteractionHitResult> hitTestInteraction(SceneGraph const& graph, Point rootPoint);
std::optional<InteractionHitResult> hitTestInteraction(
    SceneGraph const& graph, Point rootPoint,
    Reactive::SmallFn<bool(Interaction const&)> const& acceptTarget);

std::vector<ComponentKey> collectFocusableKeys(SceneGraph const& graph);

} // namespace flux::scenegraph
