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
  SceneNode* p = store.get(parent);
  if (!p) {
    return false;
  }
  auto* layer = std::get_if<LayerNode>(p);
  if (!layer) {
    return false;
  }
  layer->children.push_back(child);
  return true;
}

} // namespace

SceneGraph::SceneGraph() {
  LayerNode root{};
  root_ = store_.insert(SceneNode{std::move(root)});
  markDirty(); // First `needsRender()` must be true until `markRendered()`.
}

NodeId SceneGraph::addLayer(LayerNode node) { return addLayer(root_, std::move(node)); }

void SceneGraph::markDirty() { ++generation_; }

NodeId SceneGraph::addLayer(NodeId parent, LayerNode node) {
  SceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(SceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markDirty();
  return id;
}

NodeId SceneGraph::addRect(NodeId parent, RectNode node) {
  SceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(SceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markDirty();
  return id;
}

NodeId SceneGraph::addText(NodeId parent, TextNode node) {
  SceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(SceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markDirty();
  return id;
}

NodeId SceneGraph::addImage(NodeId parent, ImageNode node) {
  SceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(SceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markDirty();
  return id;
}

NodeId SceneGraph::addPath(NodeId parent, PathNode node) {
  SceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(SceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markDirty();
  return id;
}

NodeId SceneGraph::addLine(NodeId parent, LineNode node) {
  SceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(SceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  markDirty();
  return id;
}

NodeId SceneGraph::addCustomRender(NodeId parent, CustomRenderNode node) {
  SceneNode* p = store_.get(parent);
  if (!p || !std::get_if<LayerNode>(p)) {
    return kInvalidNodeId;
  }
  NodeId const id = store_.insert(SceneNode{std::move(node)});
  if (!appendChild(store_, parent, id)) {
    return kInvalidNodeId;
  }
  ++customRenderCount_;
  markDirty();
  return id;
}

// DFS over the tree; O(subtree size). Mutations call this often — if rebuilds become hot, cache a
// parent pointer per slot (or reverse map) so remove/reparent stay O(1) for parent lookup.
std::optional<NodeId> SceneGraph::parentOf(NodeId child) const {
  if (child == root_) {
    return std::nullopt;
  }
  return findParent(root_, child);
}

std::optional<NodeId> SceneGraph::findParent(NodeId subtree, NodeId target) const {
  SceneNode const* sn = get(subtree);
  auto const* layer = std::get_if<LayerNode>(sn);
  if (!layer) {
    return std::nullopt;
  }
  for (NodeId c : layer->children) {
    if (c == target) {
      return subtree;
    }
  }
  for (NodeId c : layer->children) {
    if (std::optional<NodeId> p = findParent(c, target)) {
      return p;
    }
  }
  return std::nullopt;
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
  SceneNode const* sn = get(ancestor);
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

void SceneGraph::removeRecursive(NodeId id) {
  SceneNode* sn = store_.get(id);
  if (!sn) {
    return;
  }
  if (std::holds_alternative<CustomRenderNode>(*sn)) {
    if (customRenderCount_ > 0) {
      --customRenderCount_;
    }
  }
  if (auto* layer = std::get_if<LayerNode>(sn)) {
    // Copy: each recursive step mutates this layer's `children` via eraseFromParentChildren, so we
    // must not iterate the live vector.
    std::vector<NodeId> const ch = layer->children;
    for (NodeId c : ch) {
      removeRecursive(c);
    }
    layer->children.clear();
  }
  if (id != root_) {
    if (std::optional<NodeId> p = findParent(root_, id)) {
      eraseFromParentChildren(*p, id);
    }
  }
  store_.remove(id);
}

void SceneGraph::remove(NodeId id) {
  if (id == root_) {
    return;
  }
  removeRecursive(id);
  markDirty();
}

void SceneGraph::clear() {
  auto* rootLayer = node<LayerNode>(root_);
  if (!rootLayer) {
    return;
  }
  std::vector<NodeId> const children = rootLayer->children;
  for (NodeId c : children) {
    removeRecursive(c);
  }
  customRenderCount_ = 0;
  markDirty();
}

void SceneGraph::clearChildren(NodeId parentId) {
  LayerNode* parent = node<LayerNode>(parentId);
  if (!parent) {
    return;
  }
  std::vector<NodeId> const ch = parent->children;
  for (NodeId c : ch) {
    removeRecursive(c);
  }
  parent->children.clear();
  markDirty();
}

void SceneGraph::reparent(NodeId id, NodeId newParent, std::size_t index) {
  if (id == root_) {
    return;
  }
  SceneNode* np = store_.get(newParent);
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
  std::optional<NodeId> oldParent = findParent(root_, id);
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
  markDirty();
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
  markDirty();
}

} // namespace flux
