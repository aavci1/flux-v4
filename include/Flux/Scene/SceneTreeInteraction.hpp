#pragma once

/// \file Flux/Scene/SceneTreeInteraction.hpp
///
/// Part of the Flux public API.

#include <Flux/Scene/InteractionData.hpp>
#include <Flux/Scene/SceneTree.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace flux {

std::pair<NodeId, InteractionData const*> findInteractionById(SceneTree const& tree, NodeId id);
std::pair<NodeId, InteractionData const*> findInteractionByKey(SceneTree const& tree, ComponentKey const& key);
std::pair<NodeId, InteractionData const*> findClosestInteractionByKey(SceneTree const& tree,
                                                                      ComponentKey const& key);
std::vector<ComponentKey> collectFocusableKeys(SceneTree const& tree);

} // namespace flux
