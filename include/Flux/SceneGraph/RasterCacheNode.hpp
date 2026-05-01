#pragma once

/// \file Flux/SceneGraph/RasterCacheNode.hpp
///
/// Scene-graph boundary for subtrees opted into texture rasterization.
///
/// Current implementation note: this is a staged node. It creates the invalidation and renderer
/// boundary and suppresses PreparedRenderOps below the subtree, but subtree drawing still falls
/// through to the parent canvas until the Metal offscreen texture pass lands.

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
