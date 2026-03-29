#include <Flux/Scene/SceneRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <variant>

namespace flux {

void SceneRenderer::render(SceneGraph const& graph, Canvas& canvas, Color clearColor) const {
  canvas.clear(clearColor);
  renderNode(graph.root(), graph, canvas);
}

void SceneRenderer::renderNode(NodeId id, SceneGraph const& graph, Canvas& canvas) const {
  SceneNode const* sn = graph.get(id);
  if (!sn) {
    return;
  }
  std::visit(
      [&](auto const& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, LayerNode>) {
          renderLayer(node, graph, canvas);
        } else if constexpr (std::is_same_v<T, RectNode>) {
          canvas.drawRect(node.bounds, node.cornerRadius, node.fill, node.stroke);
        } else if constexpr (std::is_same_v<T, TextNode>) {
          if (node.layout) {
            canvas.drawTextLayout(*node.layout, node.origin);
          }
        } else if constexpr (std::is_same_v<T, ImageNode>) {
          if (node.image) {
            canvas.drawImage(*node.image, node.bounds, node.fillMode, node.cornerRadius, node.opacity);
          }
        } else if constexpr (std::is_same_v<T, PathNode>) {
          canvas.drawPath(node.path, node.fill, node.stroke);
        } else if constexpr (std::is_same_v<T, LineNode>) {
          canvas.drawLine(node.from, node.to, node.stroke);
        } else if constexpr (std::is_same_v<T, CustomRenderNode>) {
          canvas.save();
          if (node.draw) {
            node.draw(canvas);
          }
          canvas.restore();
        }
      },
      *sn);
}

void SceneRenderer::renderLayer(LayerNode const& layer, SceneGraph const& graph, Canvas& canvas) const {
  canvas.save();
  canvas.transform(layer.transform);
  canvas.setOpacity(canvas.opacity() * layer.opacity);
  canvas.setBlendMode(layer.blendMode);
  if (layer.clip.has_value()) {
    canvas.clipRect(*layer.clip);
  }
  for (NodeId childId : layer.children) {
    renderNode(childId, graph, canvas);
  }
  canvas.restore();
}

} // namespace flux
