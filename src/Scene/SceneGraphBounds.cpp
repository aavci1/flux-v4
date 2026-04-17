#include <Flux/Scene/SceneGraphBounds.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <cmath>
#include <vector>
#include <variant>

namespace flux {

namespace {

constexpr float kDetEps = 1e-12f;

Rect unionRects(Rect const& a, Rect const& b) {
  float const minx = std::min(a.x, b.x);
  float const miny = std::min(a.y, b.y);
  float const maxx = std::max(a.x + a.width, b.x + b.width);
  float const maxy = std::max(a.y + a.height, b.y + b.height);
  return Rect{minx, miny, maxx - minx, maxy - miny};
}

Rect outsetRect(Rect const& rect, float amount) {
  if (amount <= 0.f) {
    return rect;
  }
  return Rect{rect.x - amount, rect.y - amount, rect.width + amount * 2.f, rect.height + amount * 2.f};
}

Rect paintBounds(Rect const& rect, StrokeStyle const& stroke, ShadowStyle const& shadow) {
  Rect painted = rect;
  if (!stroke.isNone()) {
    painted = outsetRect(painted, std::max(0.f, stroke.width * 0.5f));
  }
  if (!shadow.isNone()) {
    Rect shadowRect = rect;
    shadowRect.x += shadow.offset.x;
    shadowRect.y += shadow.offset.y;
    shadowRect = outsetRect(shadowRect, std::max(0.f, shadow.radius));
    painted = unionRects(painted, shadowRect);
  }
  return painted;
}

void unionWorldRect(Rect& acc, bool& has, Rect const& worldRect) {
  if (!has) {
    acc = worldRect;
    has = true;
    return;
  }
  float const minx = std::min(acc.x, worldRect.x);
  float const miny = std::min(acc.y, worldRect.y);
  float const maxx = std::max(acc.x + acc.width, worldRect.x + worldRect.width);
  float const maxy = std::max(acc.y + acc.height, worldRect.y + worldRect.height);
  acc = Rect{minx, miny, maxx - minx, maxy - miny};
}

std::optional<Rect> intersectRects(Rect const& a, Rect const& b) {
  float const x0 = std::max(a.x, b.x);
  float const y0 = std::max(a.y, b.y);
  float const x1 = std::min(a.x + a.width, b.x + b.width);
  float const y1 = std::min(a.y + a.height, b.y + b.height);
  if (x1 <= x0 || y1 <= y0) {
    return std::nullopt;
  }
  return Rect{x0, y0, x1 - x0, y1 - y0};
}

std::optional<Rect> quadWorldAabb(Rect const& local, Mat3 const& worldFromLocal) {
  if (std::abs(worldFromLocal.affineDeterminant()) < kDetEps) {
    return std::nullopt;
  }
  Point const p0 = worldFromLocal.apply({local.x, local.y});
  Point const p1 = worldFromLocal.apply({local.x + local.width, local.y});
  Point const p2 = worldFromLocal.apply({local.x, local.y + local.height});
  Point const p3 = worldFromLocal.apply({local.x + local.width, local.y + local.height});
  float const minx = std::min({p0.x, p1.x, p2.x, p3.x});
  float const miny = std::min({p0.y, p1.y, p2.y, p3.y});
  float const maxx = std::max({p0.x, p1.x, p2.x, p3.x});
  float const maxy = std::max({p0.y, p1.y, p2.y, p3.y});
  return Rect{minx, miny, maxx - minx, maxy - miny};
}

void unionClippedWorldRect(Rect const& worldRect, std::optional<Rect> const& clipWorld, Rect& acc, bool& has) {
  if (clipWorld.has_value()) {
    if (std::optional<Rect> const clipped = intersectRects(worldRect, *clipWorld)) {
      unionWorldRect(acc, has, *clipped);
    }
    return;
  }
  unionWorldRect(acc, has, worldRect);
}

void accumulate(NodeId id, SceneGraph const& graph, Mat3 const& parentWorld,
                std::optional<Rect> const& clipWorld, Rect& acc, bool& has) {
  SceneNode const* sn = graph.get(id);
  if (!sn) {
    return;
  }
  if (auto const* layer = std::get_if<LayerNode>(sn)) {
    Mat3 const localWorld = parentWorld * layer->transform;
    std::optional<Rect> nextClip = clipWorld;
    if (layer->clip.has_value()) {
      if (std::optional<Rect> const layerClipWorld = quadWorldAabb(*layer->clip, localWorld)) {
        nextClip = nextClip.has_value() ? intersectRects(*nextClip, *layerClipWorld) : layerClipWorld;
      } else {
        nextClip = std::nullopt;
      }
    }
    for (NodeId c : layer->children) {
      accumulate(c, graph, localWorld, nextClip, acc, has);
    }
    return;
  }
  if (auto const* rect = std::get_if<RectNode>(sn)) {
    if (std::optional<Rect> const worldRect = quadWorldAabb(paintBounds(rect->bounds, rect->stroke, rect->shadow),
                                                            parentWorld)) {
      unionClippedWorldRect(*worldRect, clipWorld, acc, has);
    }
    return;
  }
  if (auto const* text = std::get_if<TextNode>(sn)) {
    if (!text->layout) {
      return;
    }
    Rect tb{};
    if (text->allocation.width > 0.f && text->allocation.height > 0.f) {
      tb = text->allocation;
    } else {
      Size const ms = text->layout->measuredSize;
      tb = Rect::sharp(text->origin.x, text->origin.y, ms.width, ms.height);
    }
    if (std::optional<Rect> const worldRect = quadWorldAabb(tb, parentWorld)) {
      unionClippedWorldRect(*worldRect, clipWorld, acc, has);
    }
    return;
  }
  if (auto const* img = std::get_if<ImageNode>(sn)) {
    if (std::optional<Rect> const worldRect = quadWorldAabb(img->bounds, parentWorld)) {
      unionClippedWorldRect(*worldRect, clipWorld, acc, has);
    }
    return;
  }
  if (auto const* cr = std::get_if<CustomRenderNode>(sn)) {
    if (std::optional<Rect> const worldRect = quadWorldAabb(cr->frame, parentWorld)) {
      unionClippedWorldRect(*worldRect, clipWorld, acc, has);
    }
    return;
  }
  if (auto const* path = std::get_if<PathNode>(sn)) {
    Rect const b = paintBounds(path->path.getBounds(), path->stroke, path->shadow);
    if (std::optional<Rect> const worldRect = quadWorldAabb(b, parentWorld)) {
      unionClippedWorldRect(*worldRect, clipWorld, acc, has);
    }
    return;
  }
  if (auto const* line = std::get_if<LineNode>(sn)) {
    float const minx = std::min(line->from.x, line->to.x);
    float const miny = std::min(line->from.y, line->to.y);
    float const maxx = std::max(line->from.x, line->to.x);
    float const maxy = std::max(line->from.y, line->to.y);
    float const w = std::max(0.f, maxx - minx);
    float const h = std::max(0.f, maxy - miny);
    Rect const bounds = paintBounds(Rect{minx, miny, w, h}, line->stroke, ShadowStyle::none());
    if (std::optional<Rect> const worldRect = quadWorldAabb(bounds, parentWorld)) {
      unionClippedWorldRect(*worldRect, clipWorld, acc, has);
    }
    return;
  }
}

} // namespace

Mat3 subtreeAncestorWorldTransform(SceneGraph const& graph, NodeId subtreeRoot) {
  std::vector<NodeId> path;
  NodeId cur = subtreeRoot;
  while (true) {
    path.push_back(cur);
    if (cur == graph.root()) {
      break;
    }
    auto p = graph.parentOf(cur);
    if (!p) {
      break;
    }
    cur = *p;
  }
  std::reverse(path.begin(), path.end());
  Mat3 w = Mat3::identity();
  for (std::size_t i = 1; i + 1 < path.size(); ++i) {
    if (auto const* layer = graph.node<LayerNode>(path[i])) {
      w = w * layer->transform;
    }
  }
  return w;
}

Rect unionSubtreeBounds(SceneGraph const& graph, NodeId subtreeRoot, Mat3 parentWorld) {
  Rect acc{};
  bool has = false;
  accumulate(subtreeRoot, graph, parentWorld, std::nullopt, acc, has);
  if (!has) {
    return Rect{};
  }
  return acc;
}

Rect measureRootContentBounds(SceneGraph const& graph) {
  Rect acc{};
  bool has = false;
  auto const* root = graph.node<LayerNode>(graph.root());
  if (!root) {
    return {};
  }
  Mat3 const id = Mat3::identity();
  for (NodeId c : root->children) {
    accumulate(c, graph, id, std::nullopt, acc, has);
  }
  if (!has) {
    return {};
  }
  return acc;
}

Size measureRootContentSize(SceneGraph const& graph) {
  Rect const b = measureRootContentBounds(graph);
  if (b.width <= 0.f && b.height <= 0.f) {
    return {};
  }
  return {b.width, b.height};
}

} // namespace flux
