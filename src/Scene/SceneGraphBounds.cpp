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

void quadToWorldAabb(Rect const& local, Mat3 const& worldFromLocal, Rect& acc, bool& has) {
  if (std::abs(worldFromLocal.affineDeterminant()) < kDetEps) {
    return;
  }
  Point const p0 = worldFromLocal.apply({local.x, local.y});
  Point const p1 = worldFromLocal.apply({local.x + local.width, local.y});
  Point const p2 = worldFromLocal.apply({local.x, local.y + local.height});
  Point const p3 = worldFromLocal.apply({local.x + local.width, local.y + local.height});
  float const minx = std::min({p0.x, p1.x, p2.x, p3.x});
  float const miny = std::min({p0.y, p1.y, p2.y, p3.y});
  float const maxx = std::max({p0.x, p1.x, p2.x, p3.x});
  float const maxy = std::max({p0.y, p1.y, p2.y, p3.y});
  unionWorldRect(acc, has, Rect{minx, miny, maxx - minx, maxy - miny});
}

void accumulate(NodeId id, SceneGraph const& graph, Mat3 const& parentWorld, Rect& acc, bool& has) {
  SceneNode const* sn = graph.get(id);
  if (!sn) {
    return;
  }
  if (auto const* layer = std::get_if<LayerNode>(sn)) {
    Mat3 const localWorld = parentWorld * layer->transform;
    for (NodeId c : layer->children) {
      accumulate(c, graph, localWorld, acc, has);
    }
    return;
  }
  if (auto const* rect = std::get_if<RectNode>(sn)) {
    quadToWorldAabb(rect->bounds, parentWorld, acc, has);
    return;
  }
  if (auto const* text = std::get_if<TextNode>(sn)) {
    if (!text->layout) {
      return;
    }
    Size const ms = text->layout->measuredSize;
    Rect const tb = Rect::sharp(text->origin.x, text->origin.y, ms.width, ms.height);
    quadToWorldAabb(tb, parentWorld, acc, has);
    return;
  }
  if (auto const* img = std::get_if<ImageNode>(sn)) {
    quadToWorldAabb(img->bounds, parentWorld, acc, has);
    return;
  }
  if (auto const* cr = std::get_if<CustomRenderNode>(sn)) {
    quadToWorldAabb(cr->frame, parentWorld, acc, has);
    return;
  }
  if (auto const* path = std::get_if<PathNode>(sn)) {
    Rect const b = path->path.getBounds();
    quadToWorldAabb(b, parentWorld, acc, has);
    return;
  }
  if (auto const* line = std::get_if<LineNode>(sn)) {
    float const minx = std::min(line->from.x, line->to.x);
    float const miny = std::min(line->from.y, line->to.y);
    float const maxx = std::max(line->from.x, line->to.x);
    float const maxy = std::max(line->from.y, line->to.y);
    float const w = std::max(0.f, maxx - minx);
    float const h = std::max(0.f, maxy - miny);
    quadToWorldAabb(Rect{minx, miny, w, h}, parentWorld, acc, has);
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
  accumulate(subtreeRoot, graph, parentWorld, acc, has);
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
    accumulate(c, graph, id, acc, has);
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
