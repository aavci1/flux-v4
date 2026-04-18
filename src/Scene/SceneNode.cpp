#include <Flux/Scene/SceneNode.hpp>

#include <Flux/Scene/Renderer.hpp>

#include "Scene/SceneGeometry.hpp"

#include <algorithm>
#include <cassert>

namespace flux {

std::string_view sceneNodeKindName(SceneNodeKind kind) noexcept {
  switch (kind) {
  case SceneNodeKind::Group:
    return "Group";
  case SceneNodeKind::Modifier:
    return "Modifier";
  case SceneNodeKind::Rect:
    return "Rect";
  case SceneNodeKind::Text:
    return "Text";
  case SceneNodeKind::Image:
    return "Image";
  case SceneNodeKind::Path:
    return "Path";
  case SceneNodeKind::Line:
    return "Line";
  case SceneNodeKind::Render:
    return "Render";
  case SceneNodeKind::Custom:
    return "Custom";
  }
  return "Unknown";
}

SceneNode::SceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Group, id) {}

SceneNode::SceneNode(SceneNodeKind kind, NodeId id)
    : kind_(kind)
    , id_(id) {}

SceneNode::~SceneNode() = default;

void SceneNode::adoptChild(std::unique_ptr<SceneNode> child, std::size_t index) {
  assert(child);
  child->parent_ = this;
  children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
  markBoundsDirty();
}

void SceneNode::appendChild(std::unique_ptr<SceneNode> child) {
  adoptChild(std::move(child), children_.size());
}

void SceneNode::insertChild(std::size_t index, std::unique_ptr<SceneNode> child) {
  adoptChild(std::move(child), std::min(index, children_.size()));
}

std::unique_ptr<SceneNode> SceneNode::removeChild(SceneNode* child) {
  auto it =
      std::find_if(children_.begin(), children_.end(), [child](std::unique_ptr<SceneNode> const& ptr) {
        return ptr.get() == child;
      });
  if (it == children_.end()) {
    return nullptr;
  }
  std::unique_ptr<SceneNode> out = std::move(*it);
  children_.erase(it);
  out->parent_ = nullptr;
  markBoundsDirty();
  return out;
}

std::vector<std::unique_ptr<SceneNode>> SceneNode::releaseChildren() {
  std::vector<std::unique_ptr<SceneNode>> out = std::move(children_);
  for (std::unique_ptr<SceneNode>& child : out) {
    if (child) {
      child->parent_ = nullptr;
    }
  }
  children_.clear();
  markBoundsDirty();
  return out;
}

void SceneNode::reorderChildren(std::span<SceneNode* const> order) {
  if (order.size() != children_.size()) {
    return;
  }
  std::vector<std::unique_ptr<SceneNode>> next;
  next.reserve(children_.size());
  for (SceneNode* node : order) {
    auto it = std::find_if(children_.begin(), children_.end(), [node](std::unique_ptr<SceneNode> const& ptr) {
      return ptr.get() == node;
    });
    if (it == children_.end()) {
      return;
    }
    next.push_back(std::move(*it));
  }
  children_ = std::move(next);
  markBoundsDirty();
}

void SceneNode::replaceChildren(std::vector<std::unique_ptr<SceneNode>> children) {
  children_.clear();
  children_.reserve(children.size());
  for (std::unique_ptr<SceneNode>& child : children) {
    if (!child) {
      continue;
    }
    child->parent_ = this;
    children_.push_back(std::move(child));
  }
  markBoundsDirty();
}

void SceneNode::replayLocalPaint(Renderer& renderer) const {
  for (PaintCommand const& cmd : localPaintCache_) {
    replayPaintCommand(cmd, renderer);
  }
}

void SceneNode::rebuildLocalPaint() {
  localPaintCache_.clear();
  paintDirty_ = false;
}

Rect SceneNode::computeOwnBounds() const {
  return {};
}

Rect SceneNode::adjustSubtreeBounds(Rect r) const {
  return r;
}

void SceneNode::recomputeBounds() {
  Rect subtree = computeOwnBounds();
  for (std::unique_ptr<SceneNode> const& child : children_) {
    subtree = scene::unionRect(subtree, scene::offsetRect(child->bounds, child->position));
  }
  Rect const next = adjustSubtreeBounds(subtree);
  if (next == bounds) {
    boundsDirty_ = false;
    return;
  }
  bounds = next;
  boundsDirty_ = false;
  if (parent_) {
    parent_->recomputeBounds();
  }
}

SceneNode* SceneNode::hitTest(Point local) {
  if (!bounds.contains(local)) {
    return nullptr;
  }
  for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
    Point const childLocal = local - (*it)->position;
    if (SceneNode* hit = (*it)->hitTest(childLocal)) {
      return hit;
    }
  }
  if (paints() || interaction_) {
    return this;
  }
  return nullptr;
}

SceneNode const* SceneNode::hitTest(Point local) const {
  return const_cast<SceneNode*>(this)->hitTest(local);
}

void SceneNode::setInteraction(std::unique_ptr<InteractionData> interaction) {
  interaction_ = std::move(interaction);
}

} // namespace flux
