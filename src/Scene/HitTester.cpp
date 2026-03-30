#include <Flux/Scene/HitTester.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <cmath>
#include <functional>
#include <variant>

namespace flux {

namespace {

constexpr float kDetEps = 1e-12f;

bool acceptNodeOrAll(std::function<bool(NodeId)> const* acceptTarget, NodeId id) {
  if (!acceptTarget) {
    return true;
  }
  return (*acceptTarget)(id);
}

} // namespace

std::optional<HitResult> HitTester::hitTest(SceneGraph const& graph, Point windowPoint) const {
  return hitTestNode(graph.root(), graph, windowPoint, Mat3::identity(), nullptr);
}

std::optional<HitResult> HitTester::hitTest(SceneGraph const& graph, Point windowPoint,
                                         std::function<bool(NodeId)> const& acceptTarget) const {
  return hitTestNode(graph.root(), graph, windowPoint, Mat3::identity(), &acceptTarget);
}

std::optional<Point> HitTester::localPointForNode(SceneGraph const& graph, Point windowPoint,
                                                  NodeId targetId) const {
  return localPointForNodeImpl(graph.root(), graph, windowPoint, Mat3::identity(), targetId);
}

std::optional<Point> HitTester::localPointForNodeImpl(NodeId id, SceneGraph const& graph,
                                                      Point windowPoint, Mat3 const& parentTransform,
                                                      NodeId targetId) const {
  SceneNode const* sn = graph.get(id);
  if (!sn) {
    return std::nullopt;
  }
  if (auto const* layer = std::get_if<LayerNode>(sn)) {
    Mat3 const localTransform = parentTransform * layer->transform;
    if (layer->id == targetId) {
      if (std::abs(localTransform.affineDeterminant()) < kDetEps) {
        return std::nullopt;
      }
      return localTransform.inverse().apply(windowPoint);
    }
    // Unlike hitTest, do not cull by clip: we map window points to a node's local space for drag /
    // release delivery even when the pointer is outside the clipped region (e.g. scroll viewport).
    for (NodeId child : layer->children) {
      if (auto p = localPointForNodeImpl(child, graph, windowPoint, localTransform, targetId)) {
        return p;
      }
    }
    return std::nullopt;
  }
  if (id == targetId) {
    if (std::abs(parentTransform.affineDeterminant()) < kDetEps) {
      return std::nullopt;
    }
    return parentTransform.inverse().apply(windowPoint);
  }
  return std::nullopt;
}

std::optional<HitResult> HitTester::hitTestNode(NodeId id, SceneGraph const& graph, Point point,
                                               Mat3 const& parentTransform,
                                               std::function<bool(NodeId)> const* acceptTarget) const {
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
      if (auto r = hitTestNode(*it, graph, point, localTransform, acceptTarget)) {
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
      if (acceptNodeOrAll(acceptTarget, rect->id)) {
        return HitResult{rect->id, localPoint};
      }
      // Geometry hit but filtered out: treat as pass-through so targets behind still receive input.
    }
    return std::nullopt;
  }
  if (auto const* text = std::get_if<TextNode>(sn)) {
    if (!text->layout) {
      return std::nullopt;
    }
    Rect textBounds{};
    if (text->allocation.width > 0.f && text->allocation.height > 0.f) {
      textBounds = text->allocation;
    } else {
      Size const ms = text->layout->measuredSize;
      textBounds = Rect::sharp(text->origin.x, text->origin.y, ms.width, ms.height);
    }
    if (textBounds.contains(localPoint)) {
      if (acceptNodeOrAll(acceptTarget, text->id)) {
        return HitResult{text->id, localPoint};
      }
    }
    return std::nullopt;
  }
  if (auto const* img = std::get_if<ImageNode>(sn)) {
    if (img->bounds.contains(localPoint)) {
      if (acceptNodeOrAll(acceptTarget, img->id)) {
        return HitResult{img->id, localPoint};
      }
    }
    return std::nullopt;
  }
  if (auto const* custom = std::get_if<CustomRenderNode>(sn)) {
    if (custom->frame.contains(localPoint)) {
      if (acceptNodeOrAll(acceptTarget, custom->id)) {
        return HitResult{custom->id, localPoint};
      }
    }
    return std::nullopt;
  }
  // PathNode / LineNode: geometry hit-testing not implemented; views that need hits should add a
  // Rect (e.g. PopoverCalloutShape uses a full-bounds transparent rect under the path).
  if (std::holds_alternative<PathNode>(*sn) || std::holds_alternative<LineNode>(*sn)) {
    return std::nullopt;
  }
  return std::nullopt;
}

} // namespace flux
