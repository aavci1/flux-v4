#include <Flux/Scene/LayoutOverlayRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Scene/CustomTransformSceneNode.hpp>

#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace flux {

namespace {

CornerRadius const kNoRadius{};

struct OverlayRectKey {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;

  bool operator==(OverlayRectKey const&) const = default;
};

struct OverlayRectKeyHash {
  std::size_t operator()(OverlayRectKey const& key) const noexcept {
    std::size_t h = 1469598103934665603ull;
    auto mix = [&h](std::int32_t v) {
      h ^= static_cast<std::uint32_t>(v);
      h *= 1099511628211ull;
    };
    mix(key.x);
    mix(key.y);
    mix(key.width);
    mix(key.height);
    return h;
  }
};

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

Rect transformBounds(Mat3 const& t, Rect const& r) {
  Point const p0 = t.apply({r.x, r.y});
  Point const p1 = t.apply({r.x + r.width, r.y});
  Point const p2 = t.apply({r.x, r.y + r.height});
  Point const p3 = t.apply({r.x + r.width, r.y + r.height});
  float const minX = std::min({p0.x, p1.x, p2.x, p3.x});
  float const maxX = std::max({p0.x, p1.x, p2.x, p3.x});
  float const minY = std::min({p0.y, p1.y, p2.y, p3.y});
  float const maxY = std::max({p0.y, p1.y, p2.y, p3.y});
  return Rect{minX, minY, maxX - minX, maxY - minY};
}

Mat3 nodeLocalTransform(SceneNode const& node) {
  Mat3 transform = Mat3::translate(node.position);
  if (auto const* custom = dynamic_cast<CustomTransformSceneNode const*>(&node)) {
    transform = transform * custom->transform;
  }
  return transform;
}

void renderNodeOverlay(SceneNode const& node, Mat3 const& parentTransform, Canvas& canvas,
                       std::unordered_set<OverlayRectKey, OverlayRectKeyHash>& seen) {
  Mat3 const worldTransform = parentTransform * nodeLocalTransform(node);
  Rect const bounds = transformBounds(worldTransform, node.bounds);
  if (bounds.width > 0.f && bounds.height > 0.f) {
    auto quantize = [](float value) {
      return static_cast<std::int32_t>(std::lround(value * 2.f));
    };
    OverlayRectKey const key{
        .x = quantize(bounds.x),
        .y = quantize(bounds.y),
        .width = quantize(bounds.width),
        .height = quantize(bounds.height),
    };
    if (seen.insert(key).second) {
      strokeBounds(canvas, bounds);
    }
  }
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    renderNodeOverlay(*child, worldTransform, canvas, seen);
  }
}

} // namespace

void renderLayoutOverlay(SceneTree const& tree, Canvas& canvas) {
  std::unordered_set<OverlayRectKey, OverlayRectKeyHash> seen;
  renderNodeOverlay(tree.root(), Mat3::identity(), canvas, seen);
}

} // namespace flux
