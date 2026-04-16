#include <Flux/UI/LayoutTree.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace flux {

void LayoutTree::beginBuild() {
  ++buildEpoch_;
  rootId_ = {};
  firstNodeForKey_.clear();
  activeOrder_.clear();
  activeNodesDirty_ = true;
  retiredSceneNodes_.clear();
}

void LayoutTree::endBuild() {
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    if (!slots_[i].has_value()) {
      continue;
    }
    if (slotEpoch_[i] == buildEpoch_) {
      continue;
    }
    if (!slots_[i]->sceneNodes.empty()) {
      retiredSceneNodes_.insert(retiredSceneNodes_.end(), slots_[i]->sceneNodes.begin(),
                                slots_[i]->sceneNodes.end());
    }
    slots_[i].reset();
    freeList_.push_back(i);
  }
  activeNodesDirty_ = true;
}

Rect transformWorldBounds(Mat3 const& t, Rect const& r) {
  Point const p0 = t.apply({r.x, r.y});
  Point const p1 = t.apply({r.x + r.width, r.y});
  Point const p2 = t.apply({r.x, r.y + r.height});
  Point const p3 = t.apply({r.x + r.width, r.y + r.height});
  float minX = std::min({p0.x, p1.x, p2.x, p3.x});
  float maxX = std::max({p0.x, p1.x, p2.x, p3.x});
  float minY = std::min({p0.y, p1.y, p2.y, p3.y});
  float maxY = std::max({p0.y, p1.y, p2.y, p3.y});
  return Rect{minX, minY, maxX - minX, maxY - minY};
}

void LayoutTree::rebuildActiveNodesCache() const {
  activeNodes_.clear();
  activeNodes_.reserve(activeOrder_.size());
  for (LayoutNodeId id : activeOrder_) {
    if (LayoutNode const* node = get(id)) {
      activeNodes_.push_back(*node);
    }
  }
  activeNodesDirty_ = false;
}

std::span<LayoutNode const> LayoutTree::nodes() const noexcept {
  if (activeNodesDirty_) {
    rebuildActiveNodesCache();
  }
  return activeNodes_;
}

LayoutNodeId LayoutTree::allocateNodeId() {
  if (!freeList_.empty()) {
    std::size_t const index = freeList_.back();
    freeList_.pop_back();
    return LayoutNodeId::fromIndex(index);
  }
  slots_.push_back(std::nullopt);
  slotEpoch_.push_back(0);
  return LayoutNodeId::fromIndex(slots_.size() - 1U);
}

LayoutNodeId LayoutTree::pushNode(LayoutNode&& node, LayoutNodeId parent) {
  LayoutNodeId const id = allocateNodeId();

  node.id = id;
  node.parent = parent;
  node.children.clear();
  node.reusedSubtreeThisBuild = false;
  if (parent.isValid()) {
    if (LayoutNode* p = get(parent)) {
      p->children.push_back(id);
    }
  } else {
    rootId_ = id;
  }
  if (!node.componentKey.empty()) {
    firstNodeForKey_.emplace(node.componentKey, id);
  }
  slots_[id.index()] = std::move(node);
  slotEpoch_[id.index()] = buildEpoch_;
  activeOrder_.push_back(id);
  return id;
}

bool LayoutTree::reuseSubtree(LayoutNodeId rootId, LayoutNodeId parent) {
  LayoutNode* root = get(rootId);
  if (!root) {
    return false;
  }

  auto const visit = [&](auto&& self, LayoutNodeId id) -> void {
    LayoutNode* node = get(id);
    if (!node) {
      return;
    }
    node->reusedSubtreeThisBuild = true;
    slotEpoch_[id.index()] = buildEpoch_;
    activeOrder_.push_back(id);
    if (!node->componentKey.empty()) {
      firstNodeForKey_.emplace(node->componentKey, id);
    }
    for (LayoutNodeId childId : node->children) {
      self(self, childId);
    }
  };

  root->parent = parent;
  if (parent.isValid()) {
    if (LayoutNode* p = get(parent)) {
      p->children.push_back(rootId);
    }
  } else {
    rootId_ = rootId;
  }

  visit(visit, rootId);
  return true;
}

bool LayoutTree::canTranslateSubtree(LayoutNodeId rootId) const {
  auto const visit = [&](auto&& self, LayoutNodeId id) -> bool {
    LayoutNode const* node = get(id);
    if (!node) {
      return false;
    }
    if (node->kind == LayoutNode::Kind::Container) {
      return false;
    }
    if (node->kind == LayoutNode::Kind::Modifier && node->modifierHasEffectLayer) {
      return false;
    }
    for (LayoutNodeId childId : node->children) {
      if (!self(self, childId)) {
        return false;
      }
    }
    return true;
  };
  return visit(visit, rootId);
}

void LayoutTree::translateSubtree(LayoutNodeId rootId, Vec2 delta) {
  auto const visit = [&](auto&& self, LayoutNodeId id) -> void {
    LayoutNode* node = get(id);
    if (!node) {
      return;
    }
    node->frame.x += delta.x;
    node->frame.y += delta.y;
    node->assignedFrame.x += delta.x;
    node->assignedFrame.y += delta.y;
    node->worldBounds.x += delta.x;
    node->worldBounds.y += delta.y;
    for (LayoutNodeId childId : node->children) {
      self(self, childId);
    }
  };
  visit(visit, rootId);
  activeNodesDirty_ = true;
}

std::vector<NodeId> LayoutTree::takeRetiredSceneNodes() {
  std::vector<NodeId> out;
  out.swap(retiredSceneNodes_);
  return out;
}

Rect LayoutTree::unionSubtreeWorldBounds(LayoutNodeId nodeId) const {
  LayoutNode const* root = get(nodeId);
  if (!root) {
    return {};
  }
  float minX = std::numeric_limits<float>::infinity();
  float minY = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float maxY = -std::numeric_limits<float>::infinity();

  auto const visit = [&](auto&& self, LayoutNodeId id) -> void {
    LayoutNode const* n = get(id);
    if (!n) {
      return;
    }
    Rect const& w = n->worldBounds;
    if (w.width > 0.f || w.height > 0.f || (w.x != 0.f || w.y != 0.f)) {
      minX = std::min(minX, w.x);
      minY = std::min(minY, w.y);
      maxX = std::max(maxX, w.x + w.width);
      maxY = std::max(maxY, w.y + w.height);
    }
    for (LayoutNodeId c : n->children) {
      self(self, c);
    }
  };
  visit(visit, nodeId);

  if (!std::isfinite(minX) || !std::isfinite(minY)) {
    return root->worldBounds;
  }
  return Rect{minX, minY, maxX - minX, maxY - minY};
}

std::optional<Rect> LayoutTree::rectForKey(ComponentKey const& key) const {
  auto const it = firstNodeForKey_.find(key);
  if (it == firstNodeForKey_.end()) {
    return std::nullopt;
  }
  if (LayoutNode const* n = get(it->second)) {
    return n->worldBounds;
  }
  return std::nullopt;
}

} // namespace flux
