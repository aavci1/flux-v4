#pragma once

/// \file Flux/Scene/SceneRenderer.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

#include <memory>

namespace flux {

struct LayerNode;

class Canvas;
class SceneGraph;

class SceneRenderer {
public:
  SceneRenderer();
  ~SceneRenderer();
  SceneRenderer(SceneRenderer&&) noexcept;
  SceneRenderer& operator=(SceneRenderer&&) noexcept;
  SceneRenderer(SceneRenderer const&) = delete;
  SceneRenderer& operator=(SceneRenderer const&) = delete;

  /// Record draws without changing the frame clear color (used for overlays composited after the root scene).
  void render(SceneGraph const& graph, Canvas& canvas) const;
  /// Clears to \p clearColor then draws (single full-frame pass uses this once per present).
  void render(SceneGraph const& graph, Canvas& canvas, Color clearColor) const;

private:
  struct Impl;
  void renderNode(NodeId id, SceneGraph const& graph, Canvas& canvas, bool allowLayerCache) const;
  void renderLayer(LayerNode const& layer, SceneGraph const& graph, Canvas& canvas, bool allowLayerCache) const;
  std::unique_ptr<Impl> impl_;
};

} // namespace flux
