#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

namespace flux {

namespace {

/// `NodeStore::insert` may reallocate `slots_`, invalidating any raw `SceneNode*` from `get(parent)`
/// taken before the insert. Always append children after insert, re-resolving `parent`.
bool appendChild(NodeStore& store, NodeId parent, NodeId child) {
  LegacySceneNode* p = store.get(parent);
  if (!p) {
    return false;
  }
  auto* layer = std::get_if<LayerNode>(p);
  if (!layer) {
    return false;
  }
  layer->children.push_back(child);
  store.setParent(child, parent);
  return true;
}

} // namespace

SceneGraph::SceneGraph() {
  LayerNode root{};
  root_ = store_.insert(LegacySceneNode{std::move(root)});
  store_.setPaintEpoch(root_, nextPaintEpoch_++);
}

NodeId SceneGraph::addLayer(LayerNode node) { return addLayer(root_, std::move(node)); }

NodeId SceneGraph::addLayer(NodeId parent, LayerNode node) {
  LegacySceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(LegacySceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markPaintDirtyUpwards(id);
  return id;
}

NodeId SceneGraph::addRect(NodeId parent, RectNode node) {
  LegacySceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(LegacySceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markPaintDirtyUpwards(id);
  return id;
}

NodeId SceneGraph::addText(NodeId parent, TextNode node) {
  LegacySceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(LegacySceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markPaintDirtyUpwards(id);
  return id;
}

NodeId SceneGraph::addImage(NodeId parent, ImageNode node) {
  LegacySceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(LegacySceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markPaintDirtyUpwards(id);
  return id;
}

NodeId SceneGraph::addPath(NodeId parent, PathNode node) {
  LegacySceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(LegacySceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markPaintDirtyUpwards(id);
  return id;
}

NodeId SceneGraph::addLine(NodeId parent, LineNode node) {
  LegacySceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(LegacySceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markPaintDirtyUpwards(id);
  return id;
}

NodeId SceneGraph::addCustomRender(NodeId parent, CustomRenderNode node) {
  LegacySceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(LegacySceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markPaintDirtyUpwards(id);
  return id;
}

// DFS over the tree; O(subtree size). Mutations call this often — if rebuilds become hot, cache a
// parent pointer per slot (or reverse map) so remove/reparent stay O(1) for parent lookup.
std::optional<NodeId> SceneGraph::parentOf(NodeId child) const {
  if (child == root_) {
    return std::nullopt;
  }
  NodeId const parent = store_.parentOf(child);
  if (!parent.isValid()) {
    return std::nullopt;
  }
  return parent;
}

void SceneGraph::eraseFromParentChildren(NodeId parent, NodeId child) {
  auto* layer = node<LayerNode>(parent);
  if (!layer) {
    return;
  }
  std::erase(layer->children, child);
}

bool SceneGraph::isDescendant(NodeId ancestor, NodeId possibleDescendant) const {
  if (possibleDescendant == ancestor) {
    return true;
  }
  LegacySceneNode const* sn = get(ancestor);
  auto const* layer = std::get_if<LayerNode>(sn);
  if (!layer) {
    return false;
  }
  for (NodeId c : layer->children) {
    if (isDescendant(c, possibleDescendant)) {
      return true;
    }
  }
  return false;
}

void SceneGraph::removeRecursive(NodeId id, std::optional<NodeId> parent, bool detachFromParent) {
  LegacySceneNode* sn = store_.get(id);
  if (!sn) {
    return;
  }
  if (auto* layer = std::get_if<LayerNode>(sn)) {
    // Copy: each recursive step mutates this layer's `children` via eraseFromParentChildren, so we
    // must not iterate the live vector.
    std::vector<NodeId> const ch = layer->children;
    for (NodeId c : ch) {
      removeRecursive(c, id, false);
    }
    layer->children.clear();
  }
  if (detachFromParent && parent.has_value()) {
    eraseFromParentChildren(*parent, id);
  }
  store_.remove(id);
}

void SceneGraph::remove(NodeId id) {
  if (id == root_) {
    return;
  }
  std::optional<NodeId> const parent = parentOf(id);
  removeRecursive(id, parent, true);
  if (parent) {
    markPaintDirtyUpwards(*parent);
  }
}

void SceneGraph::clear() {
  auto* rootLayer = node<LayerNode>(root_);
  if (!rootLayer) {
    return;
  }
  std::vector<NodeId> const children = rootLayer->children;
  for (NodeId c : children) {
    removeRecursive(c, root_, false);
  }
  rootLayer->children.clear();
  markPaintDirtyUpwards(root_);
}

void SceneGraph::reparent(NodeId id, NodeId newParent, std::size_t index) {
  if (id == root_) {
    return;
  }
  LegacySceneNode* np = store_.get(newParent);
  if (!np) {
    return;
  }
  auto* newLayer = std::get_if<LayerNode>(np);
  if (!newLayer) {
    return;
  }
  if (newParent == id || isDescendant(id, newParent)) {
    return;
  }
  std::optional<NodeId> oldParent = parentOf(id);
  if (!oldParent) {
    return;
  }
  // `newLayer` points at `slots_[newParent.index]`; `eraseFromParentChildren` only mutates a
  // *different* layer's `children` vector (or the same layer's list when oldParent == newParent).
  // It does not reallocate `slots_`, so this pointer stays valid — no need to re-resolve after erase.
  eraseFromParentChildren(*oldParent, id);
  if (index == npos || index > newLayer->children.size()) {
    newLayer->children.push_back(id);
  } else {
    newLayer->children.insert(newLayer->children.begin() + static_cast<std::ptrdiff_t>(index), id);
  }
  store_.setParent(id, newParent);
  markPaintDirtyUpwards(id);
  markPaintDirtyUpwards(*oldParent);
  if (newParent != *oldParent) {
    markPaintDirtyUpwards(newParent);
  }
}

void SceneGraph::reorder(NodeId parent, std::vector<NodeId> const& orderedChildren) {
  auto* layer = node<LayerNode>(parent);
  if (!layer) {
    return;
  }
  auto const& cur = layer->children;
  if (orderedChildren.size() != cur.size()) {
    return;
  }
  if (!std::is_permutation(orderedChildren.begin(), orderedChildren.end(), cur.begin())) {
    return;
  }
  layer->children = orderedChildren;
  markPaintDirtyUpwards(parent);
}

std::uint64_t SceneGraph::subtreePaintEpoch(NodeId id) const {
  return store_.paintEpochOf(id);
}

void SceneGraph::markPaintDirty(NodeId id) {
  markPaintDirtyUpwards(id);
}

void SceneGraph::markPaintDirtyUpwards(NodeId id) {
  if (!id.isValid()) {
    return;
  }
  std::uint64_t const epoch = nextPaintEpoch_++;
  NodeId current = id;
  while (current.isValid()) {
    store_.setPaintEpoch(current, epoch);
    current = store_.parentOf(current);
  }
}

} // namespace flux
