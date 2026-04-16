#include <Flux/Scene/LayoutOverlayRenderer.hpp>

#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Styles.hpp>

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

Rect overlayBounds(LayoutNode const& node, LayoutTree const& tree) {
  switch (node.kind) {
  case LayoutNode::Kind::Container:
  case LayoutNode::Kind::Modifier:
    return node.worldBounds;
  case LayoutNode::Kind::Leaf:
  case LayoutNode::Kind::Composite:
    break;
  }
  return tree.unionSubtreeWorldBounds(node.id);
}

} // namespace

void renderLayoutOverlay(LayoutTree const& tree, Canvas& canvas) {
  std::unordered_set<OverlayRectKey, OverlayRectKeyHash> seen;
  seen.reserve(tree.activeIds().size());
  for (LayoutNodeId id : tree.activeIds()) {
    LayoutNode const* node = tree.get(id);
    if (!node) {
      continue;
    }
    Rect const bounds = overlayBounds(*node, tree);
    if (bounds.width <= 0.f || bounds.height <= 0.f) {
      continue;
    }

    // Deduplicate equivalent boxes so leaf subtree unions do not redraw the same outline repeatedly.
    auto quantize = [](float value) {
      return static_cast<std::int32_t>(std::lround(value * 2.f));
    };
    OverlayRectKey const key{
        .x = quantize(bounds.x),
        .y = quantize(bounds.y),
        .width = quantize(bounds.width),
        .height = quantize(bounds.height),
    };
    if (!seen.insert(key).second) {
      continue;
    }
    strokeBounds(canvas, bounds);
  }
}

} // namespace flux
