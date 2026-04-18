#include <Flux/UI/Views/Text.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/Views/TextSupport.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>

namespace flux {

namespace {

Rect explicitLeafBox(Text const&) {
  return {};
}

std::uint64_t hashCombine(std::uint64_t seed, std::uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
  return seed;
}

std::uint64_t hashFloat(float value) {
  std::uint32_t bits{};
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::uint64_t hashBool(bool value) {
  return value ? 0x9b8d6f43a2c17e5dull : 0x1f2e3d4c5b6a7988ull;
}

std::uint64_t hashString(std::string const& value) {
  return std::hash<std::string>{}(value);
}

std::uint64_t hashColor(Color value) {
  std::uint64_t h = 0x2db4f7a681c5930eull;
  h = hashCombine(h, hashFloat(value.r));
  h = hashCombine(h, hashFloat(value.g));
  h = hashCombine(h, hashFloat(value.b));
  h = hashCombine(h, hashFloat(value.a));
  return h;
}

} // namespace

void Text::layout(LayoutContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const assigned = ctx.layoutEngine().consumeAssignedFrame();
  LayoutConstraints const& constraints = ctx.constraints();
  Rect const bounds =
      detail::resolveLeafLayoutBounds(explicitLeafBox(*this), assigned, constraints, ctx.hints());
  bool const widthExplained = bounds.width > 0.f || assigned.width <= 0.f ||
                              (std::isfinite(constraints.maxWidth) && constraints.maxWidth <= 0.f);
  bool const heightExplained = bounds.height > 0.f || assigned.height <= 0.f ||
                               (std::isfinite(constraints.maxHeight) && constraints.maxHeight <= 0.f);
  assert(text.empty() || (widthExplained && heightExplained));
#ifdef NDEBUG
  (void)widthExplained;
  (void)heightExplained;
#endif

  LayoutNode node{};
  node.kind = LayoutNode::Kind::Leaf;
  node.frame = bounds;
  node.componentKey = stableKey;
  node.element = ctx.currentElement();
  node.constraints = constraints;
  node.hints = ctx.hints();
  LayoutNodeId const id = ctx.pushLayoutNode(std::move(node));
  ctx.registerCompositeSubtreeRootIfPending(id);
  layoutDebugLogLeaf("Text", constraints, bounds, 0.f, 0.f, 0.f);
}

std::uint64_t Text::measureCacheKey() const noexcept {
  std::uint64_t h = 0x4fb3d8c2a9716e05ull;
  h = hashCombine(h, hashString(text));
  h = hashCombine(h, hashString(font.family));
  h = hashCombine(h, hashFloat(font.size));
  h = hashCombine(h, hashFloat(font.weight));
  h = hashCombine(h, hashBool(font.italic));
  h = hashCombine(h, hashColor(color));
  h = hashCombine(h, hashColor(selectionColor));
  h = hashCombine(h, hashBool(selectable));
  h = hashCombine(h, std::hash<int>{}(static_cast<int>(horizontalAlignment)));
  h = hashCombine(h, std::hash<int>{}(static_cast<int>(verticalAlignment)));
  h = hashCombine(h, std::hash<int>{}(static_cast<int>(wrapping)));
  h = hashCombine(h, std::hash<int>{}(maxLines));
  h = hashCombine(h, hashFloat(firstBaselineOffset));
  return h;
}

Size Text::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                   TextSystem& textSystem) const {
  ctx.advanceChildSlot();
  (void)hints;
  auto const [resolvedFont, resolvedColor] = text_detail::resolveBodyTextStyle(font, color);
  TextLayoutOptions const options = text_detail::makeTextLayoutOptions(*this);
  float maxWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  if (wrapping == TextWrapping::NoWrap) {
    maxWidth = 0.f;
  }
  Size size = textSystem.measure(text, resolvedFont, resolvedColor, maxWidth, options);
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    size.width = std::min(size.width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.height = std::min(size.height, constraints.maxHeight);
  }
  return size;
}

} // namespace flux
