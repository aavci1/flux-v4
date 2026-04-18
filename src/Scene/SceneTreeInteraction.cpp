#include <Flux/Scene/SceneTreeInteraction.hpp>

#include <algorithm>
#include <cstddef>

#include "Scene/SceneTraversal.hpp"

namespace flux {

namespace {

bool isPrefix(ComponentKey const& prefix, ComponentKey const& key) {
  return prefix.size() <= key.size() && std::equal(prefix.begin(), prefix.end(), key.begin());
}

void findClosestInteractionImpl(SceneNode const& node, ComponentKey const& key, NodeId& bestId,
                                InteractionData const*& bestInteraction, std::size_t& bestSharedDepth,
                                std::size_t& bestDistance) {
  if (InteractionData const* interaction = node.interaction(); interaction && !interaction->stableTargetKey.empty()) {
    bool const candidateIsAncestor = isPrefix(interaction->stableTargetKey, key);
    bool const candidateIsDescendant = isPrefix(key, interaction->stableTargetKey);
    if (candidateIsAncestor || candidateIsDescendant) {
      std::size_t const sharedDepth = std::min(interaction->stableTargetKey.size(), key.size());
      std::size_t const distance =
          interaction->stableTargetKey.size() > key.size()
              ? (interaction->stableTargetKey.size() - key.size())
              : (key.size() - interaction->stableTargetKey.size());
      if (!bestInteraction || sharedDepth > bestSharedDepth ||
          (sharedDepth == bestSharedDepth && distance < bestDistance)) {
        bestId = node.id();
        bestInteraction = interaction;
        bestSharedDepth = sharedDepth;
        bestDistance = distance;
      }
    }
  }

  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    findClosestInteractionImpl(*child, key, bestId, bestInteraction, bestSharedDepth, bestDistance);
  }
}

} // namespace

std::pair<NodeId, InteractionData const*> findInteractionById(SceneTree const& tree, NodeId id) {
  if (!id.isValid()) {
    return {kInvalidNodeId, nullptr};
  }
  SceneNode const* match = nullptr;
  scene::walkSceneTree(tree.root(), [&](SceneNode const& node) {
    if (!match && node.id() == id && node.interaction()) {
      match = &node;
    }
  });
  return match ? std::pair<NodeId, InteractionData const*>{match->id(), match->interaction()}
               : std::pair<NodeId, InteractionData const*>{kInvalidNodeId, nullptr};
}

std::pair<NodeId, InteractionData const*> findInteractionByKey(SceneTree const& tree, ComponentKey const& key) {
  if (key.empty()) {
    return {kInvalidNodeId, nullptr};
  }
  SceneNode const* match = nullptr;
  scene::walkSceneTree(tree.root(), [&](SceneNode const& node) {
    if (!match) {
      if (InteractionData const* interaction = node.interaction();
          interaction && interaction->stableTargetKey == key) {
        match = &node;
      }
    }
  });
  return match ? std::pair<NodeId, InteractionData const*>{match->id(), match->interaction()}
               : std::pair<NodeId, InteractionData const*>{kInvalidNodeId, nullptr};
}

std::pair<NodeId, InteractionData const*> findClosestInteractionByKey(SceneTree const& tree,
                                                                      ComponentKey const& key) {
  auto const exact = findInteractionByKey(tree, key);
  if (exact.second || key.empty()) {
    return exact;
  }

  NodeId bestId = kInvalidNodeId;
  InteractionData const* bestInteraction = nullptr;
  std::size_t bestSharedDepth = 0;
  std::size_t bestDistance = 0;
  findClosestInteractionImpl(tree.root(), key, bestId, bestInteraction, bestSharedDepth, bestDistance);
  return {bestId, bestInteraction};
}

std::optional<InteractionHitResult> hitTestInteraction(SceneTree const& tree, Point rootPoint) {
  return hitTestInteraction(tree, rootPoint, [](InteractionData const&) { return true; });
}

std::optional<InteractionHitResult> hitTestInteraction(
    SceneTree const& tree, Point rootPoint, std::function<bool(InteractionData const&)> const& acceptTarget) {
  if (auto hit = scene::hitTestNode(tree.root(), rootPoint, [&](SceneNode const& node) {
        if (InteractionData const* interaction = node.interaction()) {
          return acceptTarget(*interaction);
        }
        return false;
      })) {
    return InteractionHitResult{
        .nodeId = hit->first->id(),
        .localPoint = hit->second,
        .interaction = hit->first->interaction(),
    };
  }
  return std::nullopt;
}

std::vector<ComponentKey> collectFocusableKeys(SceneTree const& tree) {
  std::vector<ComponentKey> out{};
  scene::walkSceneTree(tree.root(), [&](SceneNode const& node) {
    if (InteractionData const* interaction = node.interaction();
        interaction && interaction->focusable && !interaction->stableTargetKey.empty()) {
      out.push_back(interaction->stableTargetKey);
    }
  });
  return out;
}

} // namespace flux
