#include <Flux/Scene/HitTester.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <cmath>
#include <variant>

namespace flux {

namespace {

constexpr float kDetEps = 1e-12f;

} // namespace

std::optional<HitResult> HitTester::hitTest(SceneGraph const& graph, Point windowPoint) const {
  return hitTestNode(graph.root(), graph, windowPoint, Mat3::identity());
}

std::optional<HitResult> HitTester::hitTestNode(NodeId id, SceneGraph const& graph, Point point,
                                               Mat3 const& parentTransform) const {
  SceneNode const* sn = graph.get(id);
  if (!sn) {
    return std::nullopt;
  }
  if (auto const* layer = std::get_if<LayerNode>(sn)) {
    Mat3 const localTransform = parentTransform * layer->transform;
    if (layer->clip.has_value()) {
      if (std::abs(localTransform.affineDeterminant()) < kDetEps) {
        return std::nullopt;
      }
      Point const localPoint = localTransform.inverse().apply(point);
      if (!layer->clip->contains(localPoint)) {
        return std::nullopt;
      }
    }
    for (auto it = layer->children.rbegin(); it != layer->children.rend(); ++it) {
      if (auto r = hitTestNode(*it, graph, point, localTransform)) {
        return r;
      }
    }
    return std::nullopt;
  }
  if (std::abs(parentTransform.affineDeterminant()) < kDetEps) {
    return std::nullopt;
  }
  Point const localPoint = parentTransform.inverse().apply(point);
  if (auto const* rect = std::get_if<RectNode>(sn)) {
    if (rect->bounds.contains(localPoint)) {
      return HitResult{rect->id, localPoint};
    }
    return std::nullopt;
  }
  if (auto const* text = std::get_if<TextNode>(sn)) {
    if (!text->layout) {
      return std::nullopt;
    }
    Size const ms = text->layout->measuredSize;
    Rect const textBounds =
        Rect::sharp(text->origin.x, text->origin.y, ms.width, ms.height);
    if (textBounds.contains(localPoint)) {
      return HitResult{text->id, localPoint};
    }
    return std::nullopt;
  }
  if (auto const* img = std::get_if<ImageNode>(sn)) {
    if (img->bounds.contains(localPoint)) {
      return HitResult{img->id, localPoint};
    }
    return std::nullopt;
  }
  if (std::holds_alternative<PathNode>(*sn) || std::holds_alternative<LineNode>(*sn)) {
    return std::nullopt;
  }
  return std::nullopt;
}

} // namespace flux
