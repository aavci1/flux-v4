#include <Flux/Scene/CustomTransformSceneNode.hpp>

#include <Flux/Scene/Renderer.hpp>

namespace flux {

namespace {

bool rectEmpty(Rect const& rect) {
  return rect.width == 0.f && rect.height == 0.f;
}

Rect transformBounds(Mat3 const& t, Rect const& r) {
  Point const p0 = t.apply(Point{r.x, r.y});
  Point const p1 = t.apply(Point{r.x + r.width, r.y});
  Point const p2 = t.apply(Point{r.x, r.y + r.height});
  Point const p3 = t.apply(Point{r.x + r.width, r.y + r.height});
  float const minX = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
  float const minY = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
  float const maxX = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
  float const maxY = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
  return Rect{minX, minY, maxX - minX, maxY - minY};
}

Rect offsetRect(Rect rect, Point delta) {
  rect.x += delta.x;
  rect.y += delta.y;
  return rect;
}

Rect unionRect(Rect lhs, Rect rhs) {
  if (rectEmpty(lhs)) {
    return rhs;
  }
  if (rectEmpty(rhs)) {
    return lhs;
  }
  float const x0 = std::min(lhs.x, rhs.x);
  float const y0 = std::min(lhs.y, rhs.y);
  float const x1 = std::max(lhs.x + lhs.width, rhs.x + rhs.width);
  float const y1 = std::max(lhs.y + lhs.height, rhs.y + rhs.height);
  return Rect{x0, y0, x1 - x0, y1 - y0};
}

} // namespace

CustomTransformSceneNode::CustomTransformSceneNode(NodeId id)
    : SceneNode(SceneNodeKind::Custom, id) {}

void CustomTransformSceneNode::applyNodeState(Renderer& renderer) const {
  renderer.transform(transform);
}

void CustomTransformSceneNode::recomputeBounds() {
  Rect subtree{};
  for (std::unique_ptr<SceneNode> const& child : children()) {
    subtree = unionRect(subtree, transformBounds(transform, offsetRect(child->bounds, child->position)));
  }
  if (subtree == bounds) {
    clearBoundsDirty();
    return;
  }
  bounds = subtree;
  clearBoundsDirty();
  if (parent()) {
    parent()->recomputeBounds();
  }
}

SceneNode* CustomTransformSceneNode::hitTest(Point local) {
  if (children().empty()) {
    return nullptr;
  }
  Point const childLocal = transform.inverse().apply(local);
  return children().front()->hitTest(childLocal);
}

SceneNode const* CustomTransformSceneNode::hitTest(Point local) const {
  return const_cast<CustomTransformSceneNode*>(this)->hitTest(local);
}

} // namespace flux
