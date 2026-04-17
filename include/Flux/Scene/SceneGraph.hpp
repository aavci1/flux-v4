#pragma once

/// \file Flux/Scene/SceneGraph.hpp
///
/// Part of the Flux public API.


#include <Flux/Scene/NodeId.hpp>
#include <Flux/Scene/NodeStore.hpp>
#include <Flux/Scene/Nodes.hpp>

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace flux {

class SceneRenderer;

class SceneGraph {
public:
  static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

  explicit SceneGraph();
  ~SceneGraph() = default;
  SceneGraph(SceneGraph const&) = delete;
  SceneGraph& operator=(SceneGraph const&) = delete;
  SceneGraph(SceneGraph&&) noexcept = default;
  SceneGraph& operator=(SceneGraph&&) noexcept = default;

  /// Adds a layer under the scene root (same as `addLayer(root(), node)`).
  NodeId addLayer(LayerNode node);
  NodeId addLayer(NodeId parent, LayerNode node);
  NodeId addRect(NodeId parent, RectNode node);
  NodeId addText(NodeId parent, TextNode node);
  NodeId addImage(NodeId parent, ImageNode node);
  NodeId addPath(NodeId parent, PathNode node);
  NodeId addLine(NodeId parent, LineNode node);
  NodeId addCustomRender(NodeId parent, CustomRenderNode node);

  void remove(NodeId id);
  /// Removes every node under the root except the root itself (invalidates all other `NodeId`s).
  void clear();

  void reparent(NodeId id, NodeId newParent, std::size_t index = npos);
  /// No-op unless `orderedChildren` is a permutation of the parent's current children (same set of
  /// child ids; well-formed trees have no duplicates).
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
  std::uint64_t subtreePaintEpoch(NodeId id) const;
  void markPaintDirty(NodeId id);

  /// Parent of \p child in the tree (`nullopt` if \p child is the root or unknown).
  std::optional<NodeId> parentOf(NodeId child) const;

private:
  friend class SceneRenderer;

  void const* cacheIdentity() const noexcept { return lifetime_.get(); }
  std::weak_ptr<void const> cacheLifetime() const noexcept { return lifetime_; }

  bool isDescendant(NodeId ancestor, NodeId possibleDescendant) const;
  void removeRecursive(NodeId id, std::optional<NodeId> parent, bool detachFromParent);
  void eraseFromParentChildren(NodeId parent, NodeId child);
  void markPaintDirtyUpwards(NodeId id);

  std::shared_ptr<char> lifetime_ = std::make_shared<char>();
  NodeStore store_{};
  NodeId root_{};
  std::uint64_t nextPaintEpoch_{1};
};

} // namespace flux
