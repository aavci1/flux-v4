#include <Flux/SceneGraph/SceneNode.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace flux::scenegraph {

std::string_view sceneNodeKindName(SceneNodeKind kind) noexcept {
  switch (kind) {
  case SceneNodeKind::Group:
    return "Group";
  case SceneNodeKind::Rect:
    return "Rect";
  case SceneNodeKind::Text:
    return "Text";
  case SceneNodeKind::Line:
    return "Line";
  case SceneNodeKind::Path:
    return "Path";
  case SceneNodeKind::Image:
    return "Image";
  }
  return "Unknown";
}

SceneNode::SceneNode(SceneNodeKind kind, Rect bounds)
    : bounds(bounds), kind_(kind) {}

void SceneNode::appendChild(std::unique_ptr<SceneNode> child) {
  adoptChild(std::move(child), children_.size());
}

void SceneNode::insertChild(std::size_t index, std::unique_ptr<SceneNode> child) {
  if (index > children_.size()) {
    throw std::out_of_range("SceneNode::insertChild index out of range");
  }
  adoptChild(std::move(child), index);
}

std::unique_ptr<SceneNode> SceneNode::removeChild(SceneNode& child) {
  auto it = std::find_if(children_.begin(), children_.end(),
                         [&](std::unique_ptr<SceneNode> const& current) {
                           return current.get() == &child;
                         });
  if (it == children_.end()) {
    return nullptr;
  }

  std::unique_ptr<SceneNode> removed = std::move(*it);
  children_.erase(it);
  removed->parent_ = nullptr;
  return removed;
}

std::vector<std::unique_ptr<SceneNode>> SceneNode::releaseChildren() {
  std::vector<std::unique_ptr<SceneNode>> released = std::move(children_);
  for (std::unique_ptr<SceneNode>& child : released) {
    child->parent_ = nullptr;
  }
  children_.clear();
  return released;
}

void SceneNode::replaceChildren(std::vector<std::unique_ptr<SceneNode>> children) {
  for (std::unique_ptr<SceneNode>& child : children_) {
    child->parent_ = nullptr;
  }
  children_.clear();
  children_.reserve(children.size());
  for (std::unique_ptr<SceneNode>& child : children) {
    appendChild(std::move(child));
  }
}

Rect SceneNode::localBounds() const noexcept {
  return Rect::sharp(0.f, 0.f, bounds.width, bounds.height);
}

void SceneNode::render(Renderer&) const {}

void SceneNode::adoptChild(std::unique_ptr<SceneNode> child, std::size_t index) {
  if (!child) {
    throw std::invalid_argument("SceneNode child must not be null");
  }
  if (child->parent_) {
    throw std::invalid_argument("SceneNode child already has a parent");
  }

  child->parent_ = this;
  children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(index), std::move(child));
}

void RectNode::render(Renderer& renderer) const {
  renderer.drawRect(localBounds(), cornerRadius, fill, stroke, shadow);
}

void TextNode::render(Renderer& renderer) const {
  if (!layout) {
    return;
  }
  renderer.drawTextLayout(*layout, origin);
}

void LineNode::render(Renderer& renderer) const {
  renderer.drawLine(from, to, stroke);
}

void PathNode::render(Renderer& renderer) const {
  renderer.drawPath(path, fill, stroke, shadow);
}

void ImageNode::render(Renderer& renderer) const {
  if (!image) {
    return;
  }
  renderer.drawImage(*image, localBounds(), fillMode, cornerRadius, opacity);
}

} // namespace flux::scenegraph
