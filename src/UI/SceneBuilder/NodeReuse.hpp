#pragma once

#include <Flux/Scene/SceneNode.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

namespace flux::detail {

using ReusableSceneNodes = std::unordered_map<NodeId, std::unique_ptr<SceneNode>, ::flux::NodeIdHash>;

inline ReusableSceneNodes collectReusableSceneNodes(std::vector<std::unique_ptr<SceneNode>> children) {
  ReusableSceneNodes reusable{};
  reusable.reserve(children.size());
  for (std::unique_ptr<SceneNode>& child : children) {
    if (child) {
      reusable.emplace(child->id(), std::move(child));
    }
  }
  return reusable;
}

inline ReusableSceneNodes releaseReusableChildren(SceneNode& parent) {
  return collectReusableSceneNodes(parent.releaseChildren());
}

inline std::unique_ptr<SceneNode> takeReusableNode(ReusableSceneNodes& reusable, NodeId id) {
  if (auto it = reusable.find(id); it != reusable.end()) {
    std::unique_ptr<SceneNode> node = std::move(it->second);
    reusable.erase(it);
    return node;
  }
  return nullptr;
}

template<typename T>
std::unique_ptr<T> takeReusableNodeAs(ReusableSceneNodes& reusable, NodeId id) {
  std::unique_ptr<SceneNode> node = takeReusableNode(reusable, id);
  if (!node) {
    return nullptr;
  }
  if (T* typed = dynamic_cast<T*>(node.get())) {
    node.release();
    return std::unique_ptr<T>(typed);
  }
  return nullptr;
}

} // namespace flux::detail
