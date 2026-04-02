#include <Flux/Scene/LayoutOverlayRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <cmath>
#include <variant>

namespace flux {

namespace {

CornerRadius const kNoRadius{};

StrokeStyle overlayStroke() {
  // Magenta, semi-transparent — readable on light and dark backgrounds.
  return StrokeStyle::solid(Color{0.85f, 0.2f, 0.95f, 0.55f}, 1.f);
}

void strokeBounds(Canvas& canvas, Rect const& r) {
  if (r.width <= 0.f || r.height <= 0.f) {
    return;
  }
  canvas.drawRect(r, kNoRadius, FillStyle::none(), overlayStroke());
}

void overlayNode(NodeId id, SceneGraph const& graph, Canvas& canvas) {
  SceneNode const* sn = graph.get(id);
  if (!sn) {
    return;
  }
  std::visit(
      [&](auto const& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, LayerNode>) {
          canvas.save();
          canvas.transform(node.transform);
          canvas.setOpacity(canvas.opacity() * node.opacity);
          canvas.setBlendMode(node.blendMode);
          for (NodeId childId : node.children) {
            overlayNode(childId, graph, canvas);
          }
          canvas.restore();
        } else if constexpr (std::is_same_v<T, RectNode>) {
          strokeBounds(canvas, node.bounds);
        } else if constexpr (std::is_same_v<T, TextNode>) {
          if (node.layout) {
            Rect r{};
            if (node.allocation.width > 0.f && node.allocation.height > 0.f) {
              r = node.allocation;
            } else {
              Size const ms = node.layout->measuredSize;
              r = Rect{node.origin.x, node.origin.y, ms.width, ms.height};
            }
            strokeBounds(canvas, r);
          }
        } else if constexpr (std::is_same_v<T, ImageNode>) {
          strokeBounds(canvas, node.bounds);
        } else if constexpr (std::is_same_v<T, PathNode>) {
          Rect const b = node.path.getBounds();
          strokeBounds(canvas, b);
        } else if constexpr (std::is_same_v<T, LineNode>) {
          float const minX = std::min(node.from.x, node.to.x);
          float const minY = std::min(node.from.y, node.to.y);
          float const maxX = std::max(node.from.x, node.to.x);
          float const maxY = std::max(node.from.y, node.to.y);
          float const pad = 2.f;
          strokeBounds(canvas,
                       Rect{minX - pad, minY - pad, std::max(maxX - minX + 2.f * pad, 1.f),
                            std::max(maxY - minY + 2.f * pad, 1.f)});
        } else if constexpr (std::is_same_v<T, CustomRenderNode>) {
          strokeBounds(canvas, node.frame);
        }
      },
      *sn);
}

} // namespace

void renderLayoutOverlay(SceneGraph const& graph, Canvas& canvas) {
  overlayNode(graph.root(), graph, canvas);
}

} // namespace flux
