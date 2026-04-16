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
  activeNodes_.clear();
}

void LayoutTree::endBuild() {
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    if (!slots_[i].has_value()) {
      continue;
    }
    if (slotEpoch_[i] == buildEpoch_) {
      continue;
    }
    if (!slots_[i]->componentKey.empty()) {
      auto const it = retainedNodeForKey_.find(slots_[i]->componentKey);
      if (it != retainedNodeForKey_.end() && it->second.index() == i) {
        retainedNodeForKey_.erase(it);
      }
    }
    slots_[i].reset();
    freeList_.push_back(i);
  }

  activeNodes_.clear();
  activeNodes_.reserve(activeOrder_.size());
  for (LayoutNodeId id : activeOrder_) {
    if (LayoutNode const* node = get(id)) {
      activeNodes_.push_back(*node);
    }
  }
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
  LayoutNodeId id{};
  if (!node.componentKey.empty()) {
    auto const existing = retainedNodeForKey_.find(node.componentKey);
    if (existing != retainedNodeForKey_.end()) {
      id = existing->second;
    }
  }
  if (!id.isValid()) {
    id = allocateNodeId();
  }

  node.id = id;
  node.parent = parent;
  node.children.clear();
  if (parent.isValid()) {
    if (LayoutNode* p = get(parent)) {
      p->children.push_back(id);
    }
  } else {
    rootId_ = id;
  }
  if (!node.componentKey.empty()) {
    firstNodeForKey_.emplace(node.componentKey, id);
    retainedNodeForKey_[node.componentKey] = id;
  }
  slots_[id.index()] = std::move(node);
  slotEpoch_[id.index()] = buildEpoch_;
  activeOrder_.push_back(id);
  activeNodes_.push_back(*slots_[id.index()]);
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
    slotEpoch_[id.index()] = buildEpoch_;
    activeOrder_.push_back(id);
    activeNodes_.push_back(*node);
    if (!node->componentKey.empty()) {
      firstNodeForKey_.emplace(node->componentKey, id);
      retainedNodeForKey_[node->componentKey] = id;
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
