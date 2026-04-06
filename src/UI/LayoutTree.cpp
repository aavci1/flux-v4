#include <Flux/UI/LayoutTree.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace flux {

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

LayoutNodeId LayoutTree::pushNode(LayoutNode&& node, LayoutNodeId parent) {
  LayoutNodeId const id{static_cast<std::uint32_t>(nodes_.size() + 1U)};
  node.id = id;
  node.parent = parent;
  if (parent.isValid()) {
    if (LayoutNode* p = get(parent)) {
      p->children.push_back(id);
    }
  } else {
    rootId_ = id;
  }
  nodes_.push_back(std::move(node));
  return id;
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
    if (n->kind == LayoutNode::Kind::Tombstone) {
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
  for (LayoutNode const& n : nodes_) {
    if (n.kind != LayoutNode::Kind::Tombstone && n.componentKey == key) {
      return n.worldBounds;
    }
  }
  return std::nullopt;
}

} // namespace flux
