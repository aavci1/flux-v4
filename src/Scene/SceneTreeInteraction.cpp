#include <Flux/Scene/SceneTreeInteraction.hpp>

#include <Flux/Scene/CustomTransformSceneNode.hpp>

#include <algorithm>
#include <cstddef>

namespace flux {

namespace {

bool isPrefix(ComponentKey const& prefix, ComponentKey const& key) {
  return prefix.size() <= key.size() && std::equal(prefix.begin(), prefix.end(), key.begin());
}

Point pointInChildSpace(SceneNode const& parent, SceneNode const& child, Point point) {
  if (auto const* transformNode = dynamic_cast<CustomTransformSceneNode const*>(&parent)) {
    return transformNode->transform.inverse().apply(point) - child.position;
  }
  return point - child.position;
}

void collectFocusableKeysImpl(SceneNode const& node, std::vector<ComponentKey>& out) {
  if (InteractionData const* interaction = node.interaction();
      interaction && interaction->focusable && !interaction->stableTargetKey.empty()) {
    out.push_back(interaction->stableTargetKey);
  }
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    collectFocusableKeysImpl(*child, out);
  }
}

std::pair<NodeId, InteractionData const*> findInteractionByIdImpl(SceneNode const& node, NodeId id) {
  if (node.id() == id && node.interaction()) {
    return {node.id(), node.interaction()};
  }
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    if (auto found = findInteractionByIdImpl(*child, id); found.second) {
      return found;
    }
  }
  return {kInvalidNodeId, nullptr};
}

std::pair<NodeId, InteractionData const*> findInteractionByKeyImpl(SceneNode const& node, ComponentKey const& key) {
  if (InteractionData const* interaction = node.interaction();
      interaction && interaction->stableTargetKey == key) {
    return {node.id(), interaction};
  }
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    if (auto found = findInteractionByKeyImpl(*child, key); found.second) {
      return found;
    }
  }
  return {kInvalidNodeId, nullptr};
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

std::optional<InteractionHitResult> hitTestInteractionImpl(
    SceneNode const& node, Point point, std::function<bool(InteractionData const&)> const* acceptTarget) {
  if (!node.bounds.contains(point)) {
    return std::nullopt;
  }
  for (auto it = node.children().rbegin(); it != node.children().rend(); ++it) {
    Point const childPoint = pointInChildSpace(node, **it, point);
    if (auto hit = hitTestInteractionImpl(**it, childPoint, acceptTarget)) {
      return hit;
    }
  }
  if (InteractionData const* interaction = node.interaction()) {
    if (!acceptTarget || (*acceptTarget)(*interaction)) {
      return InteractionHitResult{
          .nodeId = node.id(),
          .localPoint = point,
          .interaction = interaction,
      };
    }
  }
  return std::nullopt;
}

} // namespace

std::pair<NodeId, InteractionData const*> findInteractionById(SceneTree const& tree, NodeId id) {
  if (!id.isValid()) {
    return {kInvalidNodeId, nullptr};
  }
  return findInteractionByIdImpl(tree.root(), id);
}

std::pair<NodeId, InteractionData const*> findInteractionByKey(SceneTree const& tree, ComponentKey const& key) {
  if (key.empty()) {
    return {kInvalidNodeId, nullptr};
  }
  return findInteractionByKeyImpl(tree.root(), key);
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
  return hitTestInteractionImpl(tree.root(), rootPoint, &acceptTarget);
}

std::vector<ComponentKey> collectFocusableKeys(SceneTree const& tree) {
  std::vector<ComponentKey> out{};
  collectFocusableKeysImpl(tree.root(), out);
  return out;
}

} // namespace flux
