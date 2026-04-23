#include <Flux/SceneGraph/SceneNode.hpp>

#include "SceneGraph/SceneNodeInternal.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace flux::scenegraph {

struct SceneNode::Impl {
    explicit Impl(SceneNodeKind kindValue, Rect boundsValue) : kind(kindValue), bounds(boundsValue) {}

    SceneNodeKind kind;
    Rect bounds {};
    SceneNode *parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> children;
    bool dirty = true;
};

std::string_view sceneNodeKindName(SceneNodeKind kind) noexcept {
    switch (kind) {
    case SceneNodeKind::Group:
        return "Group";
  case SceneNodeKind::Rect:
    return "Rect";
  case SceneNodeKind::Text:
    return "Text";
  case SceneNodeKind::Image:
    return "Image";
  }
    return "Unknown";
}

SceneNode::SceneNode(SceneNodeKind kind, Rect bounds) : impl_(std::make_unique<Impl>(kind, bounds)) {}

SceneNode::~SceneNode() = default;

SceneNodeKind SceneNode::kind() const noexcept {
    return impl_->kind;
}

Rect SceneNode::bounds() const noexcept {
    return impl_->bounds;
}

Point SceneNode::position() const noexcept {
    return Point {impl_->bounds.x, impl_->bounds.y};
}

Size SceneNode::size() const noexcept {
    return Size {impl_->bounds.width, impl_->bounds.height};
}

bool SceneNode::isDirty() const noexcept {
    return impl_->dirty;
}

void SceneNode::setBounds(Rect bounds) {
    if (impl_->bounds == bounds) {
        return;
    }
    bool const sizeChanged = bounds.width != impl_->bounds.width ||
                             bounds.height != impl_->bounds.height;
    impl_->bounds = bounds;
    if (sizeChanged) {
        markDirty();
    }
}

void SceneNode::setPosition(Point position) {
    if (impl_->bounds.x == position.x && impl_->bounds.y == position.y) {
        return;
    }
    impl_->bounds.x = position.x;
    impl_->bounds.y = position.y;
}

void SceneNode::setSize(Size size) {
    if (impl_->bounds.width == size.width && impl_->bounds.height == size.height) {
        return;
    }
    impl_->bounds.width = size.width;
    impl_->bounds.height = size.height;
    markDirty();
}

SceneNode *SceneNode::parent() const noexcept {
    return impl_->parent;
}

std::span<std::unique_ptr<SceneNode> const> SceneNode::children() const noexcept {
    return impl_->children;
}

std::span<std::unique_ptr<SceneNode>> SceneNode::children() noexcept {
    return impl_->children;
}

void SceneNode::appendChild(std::unique_ptr<SceneNode> child) {
    if (!child) {
        throw std::invalid_argument("SceneNode child must not be null");
    }
    if (child->impl_->parent) {
        throw std::invalid_argument("SceneNode child already has a parent");
    }
    child->impl_->parent = this;
    impl_->children.push_back(std::move(child));
    markDirty();
}

void SceneNode::insertChild(std::size_t index, std::unique_ptr<SceneNode> child) {
    if (index > impl_->children.size()) {
        throw std::out_of_range("SceneNode::insertChild index out of range");
    }
    if (!child) {
        throw std::invalid_argument("SceneNode child must not be null");
    }
    if (child->impl_->parent) {
        throw std::invalid_argument("SceneNode child already has a parent");
    }
    child->impl_->parent = this;
    impl_->children.insert(impl_->children.begin() + static_cast<std::ptrdiff_t>(index),
                           std::move(child));
    markDirty();
}

std::unique_ptr<SceneNode> SceneNode::removeChild(SceneNode &child) {
    auto it = std::find_if(impl_->children.begin(), impl_->children.end(),
                           [&](std::unique_ptr<SceneNode> const &current) {
                               return current.get() == &child;
                           });
    if (it == impl_->children.end()) {
        return nullptr;
    }

    std::unique_ptr<SceneNode> removed = std::move(*it);
    impl_->children.erase(it);
    removed->impl_->parent = nullptr;
    markDirty();
    return removed;
}

std::vector<std::unique_ptr<SceneNode>> SceneNode::releaseChildren() {
    std::vector<std::unique_ptr<SceneNode>> released = std::move(impl_->children);
    for (std::unique_ptr<SceneNode> &child : released) {
        child->impl_->parent = nullptr;
    }
    impl_->children.clear();
    markDirty();
    return released;
}

void SceneNode::replaceChildren(std::vector<std::unique_ptr<SceneNode>> children) {
    for (std::unique_ptr<SceneNode> const &child : children) {
        if (!child) {
            throw std::invalid_argument("SceneNode child must not be null");
        }
        if (child->impl_->parent) {
            throw std::invalid_argument("SceneNode child already has a parent");
        }
    }

    for (std::unique_ptr<SceneNode> &child : impl_->children) {
        child->impl_->parent = nullptr;
    }
    impl_->children.clear();
    impl_->children.reserve(children.size());
    for (std::unique_ptr<SceneNode> &child : children) {
        child->impl_->parent = this;
        impl_->children.push_back(std::move(child));
    }
    markDirty();
}

Rect SceneNode::localBounds() const noexcept {
    return Rect::sharp(0.f, 0.f, impl_->bounds.width, impl_->bounds.height);
}

void SceneNode::render(Renderer &) const {}

void SceneNode::markDirty() noexcept {
    impl_->dirty = true;
}

void detail::SceneNodeAccess::clearDirty(SceneNode const &node) noexcept {
    node.impl_->dirty = false;
}

} // namespace flux::scenegraph
