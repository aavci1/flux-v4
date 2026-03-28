#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Scene/NodeId.hpp>

namespace flux {

struct LayerNode;

class Canvas;
class SceneGraph;

class SceneRenderer {
public:
  void render(SceneGraph const& graph, Canvas& canvas, Color clearColor = Colors::transparent) const;

private:
  void renderNode(NodeId id, SceneGraph const& graph, Canvas& canvas) const;
  void renderLayer(LayerNode const& layer, SceneGraph const& graph, Canvas& canvas) const;
};

} // namespace flux
