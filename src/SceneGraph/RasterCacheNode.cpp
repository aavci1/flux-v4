#include <Flux/SceneGraph/RasterCacheNode.hpp>

#include <Flux/SceneGraph/Renderer.hpp>

#include <algorithm>
#include <memory>

namespace flux::scenegraph {

RasterCacheNode::RasterCacheNode(Rect bounds)
    : SceneNode(SceneNodeKind::RasterCache, bounds) {}

RasterCacheNode::~RasterCacheNode() = default;

void RasterCacheNode::setSubtree(std::unique_ptr<SceneNode> subtreeNode) {
  if (!subtreeNode) {
    return;
  }
  subtreeNode->setPosition(Point{});
  replaceChildren({});
  appendChild(std::move(subtreeNode));
  setRelayout([this](LayoutConstraints const& constraints) {
    SceneNode* child = subtree();
    if (!child) {
      setSize(Size{});
      return;
    }
    child->relayout(constraints);
    child->setPosition(Point{});
    setSize(child->size());
  });
}

SceneNode* RasterCacheNode::subtree() noexcept {
  auto children = this->children();
  return children.empty() ? nullptr : children.front().get();
}

SceneNode const* RasterCacheNode::subtree() const noexcept {
  auto children = this->children();
  return children.empty() ? nullptr : children.front().get();
}

void RasterCacheNode::invalidateCache() {
  markDirty();
}

void RasterCacheNode::render(Renderer&) const {
  // The renderer owns raster-cache traversal. The node itself has no direct paint op.
}

bool RasterCacheNode::canPrepareRenderOps() const noexcept {
  return false;
}

} // namespace flux::scenegraph
