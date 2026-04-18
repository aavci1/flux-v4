#include <Flux/Scene/SceneTree.hpp>

#include "Scene/SceneGeometry.hpp"

namespace flux {

namespace {

std::uint64_t hashCombine64(std::uint64_t seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

} // namespace

SceneTree::SceneTree()
    : root_(std::make_unique<SceneNode>(NodeId{1ull})) {}

SceneTree::SceneTree(std::unique_ptr<SceneNode> root)
    : root_(std::move(root)) {
  if (!root_) {
    root_ = std::make_unique<SceneNode>(NodeId{1ull});
  }
}

SceneTree::~SceneTree() = default;
SceneTree::SceneTree(SceneTree&&) noexcept = default;
SceneTree& SceneTree::operator=(SceneTree&&) noexcept = default;

std::unique_ptr<SceneNode> SceneTree::takeRoot() {
  std::unique_ptr<SceneNode> root = std::move(root_);
  root_ = std::make_unique<SceneNode>(NodeId{1ull});
  return root;
}

void SceneTree::setRoot(std::unique_ptr<SceneNode> root) {
  root_ = std::move(root);
  if (!root_) {
    root_ = std::make_unique<SceneNode>(NodeId{1ull});
  }
}

void SceneTree::clear() {
  root_ = std::make_unique<SceneNode>(NodeId{1ull});
}

NodeId SceneTree::childId(NodeId parent, LocalId local) noexcept {
  std::uint64_t seed = parent.value == 0 ? 0x9f47b2d1aa61c6e1ull : parent.value;
  seed = hashCombine64(seed, static_cast<std::uint64_t>(local.kind));
  seed = hashCombine64(seed, local.value);
  if (seed == 0) {
    seed = 1;
  }
  return NodeId{seed};
}

Rect measureRootContentBounds(SceneTree const& tree) {
  Rect bounds{};
  for (std::unique_ptr<SceneNode> const& child : tree.root().children()) {
    bounds = scene::unionRect(bounds, scene::offsetRect(child->bounds, child->position));
  }
  return bounds;
}

Size measureRootContentSize(SceneTree const& tree) {
  Rect const bounds = measureRootContentBounds(tree);
  return Size{bounds.width, bounds.height};
}

} // namespace flux
