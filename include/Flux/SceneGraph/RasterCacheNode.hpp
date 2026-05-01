#pragma once

/// \file Flux/SceneGraph/RasterCacheNode.hpp
///
/// Scene-graph boundary for subtrees opted into texture rasterization.

#include <Flux/SceneGraph/SceneNode.hpp>

#include <memory>

namespace flux::scenegraph {

class RasterCacheNode final : public SceneNode {
public:
  explicit RasterCacheNode(Rect bounds = {});
  ~RasterCacheNode() override;

  void setSubtree(std::unique_ptr<SceneNode> subtree);
  SceneNode* subtree() noexcept;
  SceneNode const* subtree() const noexcept;

  void invalidateCache();

  void render(Renderer& renderer) const override;
  bool canPrepareRenderOps() const noexcept override;
};

} // namespace flux::scenegraph
