#include <Flux/SceneGraph/SceneInteraction.hpp>

#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/SceneGraph/SceneTraversal.hpp>

#include "Debug/PerfCounters.hpp"

#include <cstddef>

namespace flux::scenegraph {

std::pair<SceneNode const*, Interaction const*> findInteractionByKey(SceneGraph const& graph,
                                                                     ComponentKey const& key) {
    if (key.empty()) {
        return {nullptr, nullptr};
    }

    SceneNode const* match = nullptr;
    walkSceneGraph(graph.root(), [&](SceneNode const& node) {
        if (!match) {
            if (Interaction const* interaction = node.interaction();
                interaction && interaction->stableTargetKey() == key) {
                match = &node;
            }
        }
    });
    return match ? std::pair<SceneNode const*, Interaction const*>{match, match->interaction()}
                 : std::pair<SceneNode const*, Interaction const*>{nullptr, nullptr};
}

std::optional<InteractionHitResult> hitTestInteraction(SceneGraph const& graph, Point rootPoint) {
    return hitTestInteraction(graph, rootPoint, [](Interaction const&) { return true; });
}

std::optional<InteractionHitResult> hitTestInteraction(
    SceneGraph const& graph, Point rootPoint,
    Reactive::SmallFn<bool(Interaction const&)> const& acceptTarget) {
    if (auto hit = hitTestNode(graph.root(), rootPoint, [&](SceneNode const& node) {
            if (Interaction const* interaction = node.interaction()) {
                return acceptTarget(*interaction);
            }
            return false;
        })) {
        return InteractionHitResult{
            .node = hit->first,
            .localPoint = hit->second,
            .interaction = hit->first->interaction(),
        };
    }
    return std::nullopt;
}

std::vector<ComponentKey> collectFocusableKeys(SceneGraph const& graph) {
    std::vector<ComponentKey> out{};
    walkSceneGraph(graph.root(), [&](SceneNode const& node) {
        if (Interaction const* interaction = node.interaction();
            interaction && interaction->focusable() && !interaction->stableTargetKey().empty()) {
            out.push_back(interaction->stableTargetKey());
        }
    });
    return out;
}

} // namespace flux::scenegraph
