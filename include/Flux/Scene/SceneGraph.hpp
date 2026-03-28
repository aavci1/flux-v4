#pragma once

#include <Flux/Scene/NodeId.hpp>
#include <Flux/Scene/NodeStore.hpp>
#include <Flux/Scene/Nodes.hpp>

#include <cstddef>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace flux {

class SceneGraph {
public:
  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

  explicit SceneGraph();

  /// Adds a layer under the scene root (same as `addLayer(root(), node)`).
  NodeId addLayer(LayerNode node);
  NodeId addLayer(NodeId parent, LayerNode node);
  NodeId addRect(NodeId parent, RectNode node);
  NodeId addText(NodeId parent, TextNode node);
  NodeId addImage(NodeId parent, ImageNode node);
  NodeId addPath(NodeId parent, PathNode node);
  NodeId addLine(NodeId parent, LineNode node);

  void remove(NodeId id);
  void reparent(NodeId id, NodeId newParent, std::size_t index = npos);
  /// No-op unless `orderedChildren` is the same multiset (same size, same members) as the current list.
  void reorder(NodeId parent, std::vector<NodeId> const& orderedChildren);

  template <typename T>
  T* node(NodeId id) {
    SceneNode* sn = store_.get(id);
    if (!sn) {
      return nullptr;
    }
    return std::get_if<T>(sn);
  }

  template <typename T>
  T const* node(NodeId id) const {
    SceneNode const* sn = store_.get(id);
    if (!sn) {
      return nullptr;
    }
    return std::get_if<T>(sn);
  }

  SceneNode* get(NodeId id) { return store_.get(id); }
  SceneNode const* get(NodeId id) const { return store_.get(id); }

  NodeId root() const { return root_; }

private:
  std::optional<NodeId> findParent(NodeId subtree, NodeId target) const;
  bool isDescendant(NodeId ancestor, NodeId possibleDescendant) const;
  void removeRecursive(NodeId id);
  void eraseFromParentChildren(NodeId parent, NodeId child);

  NodeStore store_{};
  NodeId root_{};
};

} // namespace flux
