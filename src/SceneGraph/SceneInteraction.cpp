#include <Flux/SceneGraph/SceneInteraction.hpp>

#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/SceneGraph/SceneTraversal.hpp>

#include "Debug/PerfCounters.hpp"

#include <algorithm>
#include <cstddef>

namespace flux::scenegraph {

namespace {

bool isPrefix(ComponentKey const& prefix, ComponentKey const& key) {
    debug::perf::recordComponentKeyPrefixCompare(prefix.size());
    return key.hasPrefix(prefix);
}

void findClosestInteractionImpl(SceneNode const& node, ComponentKey const& key,
                                SceneNode const*& bestNode,
                                InteractionData const*& bestInteraction,
                                std::size_t& bestSharedDepth, std::size_t& bestDistance) {
    if (InteractionData const* interaction = node.interaction();
        interaction && !interaction->stableTargetKey.empty()) {
        bool const candidateIsAncestor = isPrefix(interaction->stableTargetKey, key);
        bool const candidateIsDescendant = isPrefix(key, interaction->stableTargetKey);
        if (candidateIsAncestor || candidateIsDescendant) {
            std::size_t const sharedDepth =
                std::min(interaction->stableTargetKey.size(), key.size());
            std::size_t const distance =
                interaction->stableTargetKey.size() > key.size()
                    ? (interaction->stableTargetKey.size() - key.size())
                    : (key.size() - interaction->stableTargetKey.size());
            if (!bestInteraction || sharedDepth > bestSharedDepth ||
                (sharedDepth == bestSharedDepth && distance < bestDistance)) {
                bestNode = &node;
                bestInteraction = interaction;
                bestSharedDepth = sharedDepth;
                bestDistance = distance;
            }
        }
    }

    for (std::unique_ptr<SceneNode> const& child : node.children()) {
        findClosestInteractionImpl(*child, key, bestNode, bestInteraction, bestSharedDepth,
                                   bestDistance);
    }
}

} // namespace

std::pair<SceneNode const*, InteractionData const*> findInteractionByKey(SceneGraph const& graph,
                                                                         ComponentKey const& key) {
    if (key.empty()) {
        return {nullptr, nullptr};
    }

    SceneNode const* match = nullptr;
    walkSceneGraph(graph.root(), [&](SceneNode const& node) {
        if (!match) {
            if (InteractionData const* interaction = node.interaction();
                interaction && interaction->stableTargetKey == key) {
                match = &node;
            }
        }
    });
    return match ? std::pair<SceneNode const*, InteractionData const*>{match, match->interaction()}
                 : std::pair<SceneNode const*, InteractionData const*>{nullptr, nullptr};
}

std::pair<SceneNode const*, InteractionData const*> findClosestInteractionByKey(
    SceneGraph const& graph, ComponentKey const& key) {
    auto const exact = findInteractionByKey(graph, key);
    if (exact.second || key.empty()) {
        return exact;
    }

    SceneNode const* bestNode = nullptr;
    InteractionData const* bestInteraction = nullptr;
    std::size_t bestSharedDepth = 0;
    std::size_t bestDistance = 0;
    findClosestInteractionImpl(graph.root(), key, bestNode, bestInteraction, bestSharedDepth,
                               bestDistance);
    return {bestNode, bestInteraction};
}

std::optional<InteractionHitResult> hitTestInteraction(SceneGraph const& graph, Point rootPoint) {
    return hitTestInteraction(graph, rootPoint, [](InteractionData const&) { return true; });
}

std::optional<InteractionHitResult> hitTestInteraction(
    SceneGraph const& graph, Point rootPoint,
    std::function<bool(InteractionData const&)> const& acceptTarget) {
    if (auto hit = hitTestNode(graph.root(), rootPoint, [&](SceneNode const& node) {
            if (InteractionData const* interaction = node.interaction()) {
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
        if (InteractionData const* interaction = node.interaction();
            interaction && interaction->focusable && !interaction->stableTargetKey.empty()) {
            out.push_back(interaction->stableTargetKey);
        }
    });
    return out;
}

} // namespace flux::scenegraph
