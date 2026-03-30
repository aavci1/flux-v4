#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

namespace flux {

struct LayerNode;

class Canvas;
class SceneGraph;

class SceneRenderer {
public:
  /// Record draws without changing the frame clear color (used for overlays composited after the root scene).
  void render(SceneGraph const& graph, Canvas& canvas) const;
  /// Clears to \p clearColor then draws (single full-frame pass uses this once per present).
  void render(SceneGraph const& graph, Canvas& canvas, Color clearColor) const;

private:
  void renderNode(NodeId id, SceneGraph const& graph, Canvas& canvas) const;
  void renderLayer(LayerNode const& layer, SceneGraph const& graph, Canvas& canvas) const;
};

} // namespace flux
