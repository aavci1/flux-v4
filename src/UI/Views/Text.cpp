#include <Flux/UI/Views/Text.hpp>

#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/Theme.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include "UI/Detail/EventHelpers.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <utility>

namespace flux {

namespace {

TextLayoutOptions textViewLayoutOptions(Text const& v, LayoutConstraints const&, LayoutHints const&) {
  TextLayoutOptions o{};
  o.horizontalAlignment = v.horizontalAlignment;
  o.verticalAlignment = v.verticalAlignment;
  o.wrapping = v.wrapping;
  o.lineHeight = 0.f;
  o.lineHeightMultiple = 0.f;
  o.maxLines = v.maxLines;
  o.firstBaselineOffset = v.firstBaselineOffset;
  return o;
}

Rect explicitLeafBox(Text const&) {
  return {};
}

std::pair<Font, Color> resolveStyle(Text const& v) {
  Theme const& theme = useEnvironment<Theme>();
  return {
      resolveFont(v.font, theme.fontBody),
      resolveColor(v.color, theme.colorTextPrimary),
  };
}

} // namespace

using flux::detail::eventHandlersFromModifiers;
using flux::detail::shouldInsertHandlers;

void Text::layout(LayoutContext& ctx) const {
  ComponentKey const stableKey = ctx.leafComponentKey();
  ctx.advanceChildSlot();
  Rect const assigned = ctx.layoutEngine().consumeAssignedFrame();
  LayoutConstraints const& cs = ctx.constraints();
  Rect const bounds = flux::detail::resolveLeafLayoutBounds(
      explicitLeafBox(*this), assigned, cs, ctx.hints());
  // Degenerate axis is valid when the parent assigned no space there, or when constraints cap that axis at <= 0.
  bool const widthExplained = bounds.width > 0.f || assigned.width <= 0.f ||
                              (std::isfinite(cs.maxWidth) && cs.maxWidth <= 0.f);
  bool const heightExplained = bounds.height > 0.f || assigned.height <= 0.f ||
                               (std::isfinite(cs.maxHeight) && cs.maxHeight <= 0.f);
  assert(text.empty() || (widthExplained && heightExplained));
  LayoutNode n{};
  n.kind = LayoutNode::Kind::Leaf;
  n.frame = bounds;
  n.componentKey = stableKey;
  n.element = ctx.currentElement();
  n.constraints = ctx.constraints();
  n.hints = ctx.hints();
  ctx.pushLayoutNode(std::move(n));
  layoutDebugLogLeaf("Text", ctx.constraints(), bounds, detail::flexGrowOf(*this),
                     detail::flexShrinkOf(*this), detail::minMainSizeOf(*this));
}

void Text::renderFromLayout(RenderContext& ctx, LayoutNode const& node) const {
  ComponentKey const stableKey = node.componentKey;
  Rect const bounds = node.frame;
  std::shared_ptr<TextLayout const> textLayout;
  if (!text.empty()) {
    auto [font, color] = resolveStyle(*this);
    TextLayoutOptions const opts = textViewLayoutOptions(*this, ctx.constraints(), ctx.hints());
    textLayout = ctx.textSystem().layout(text, font, color, bounds, opts);
  }

  if (textLayout && !textLayout->runs.empty()) {
    NodeId const textId = ctx.graph().addText(ctx.parentLayer(), TextNode{
        .layout = textLayout,
        .origin = {bounds.x, bounds.y},
        .allocation = bounds,
    });
    if (ElementModifiers const* mods = ctx.activeElementModifiers()) {
      if (!ctx.suppressLeafModifierEvents()) {
        EventHandlers h = eventHandlersFromModifiers(*mods, stableKey, bounds);
        if (shouldInsertHandlers(h)) {
          ctx.eventMap().insert(textId, std::move(h));
        }
      }
    }
  }
}

Size Text::measure(LayoutContext& ctx, LayoutConstraints const& c, LayoutHints const& hints, TextSystem& ts) const {
  ctx.advanceChildSlot();
  auto [font, color] = resolveStyle(*this);
  TextLayoutOptions const opts = textViewLayoutOptions(*this, c, hints);
  // Match boxed layout (`layoutBoxedImpl`): NoWrap uses maxWidth 0 so Core Text measures one line.
  // A finite maxWidth here would wrap during measure (e.g. ScrollView passes viewport width) and inflate
  // height while render still lays out as nowrap.
  float mw = std::isfinite(c.maxWidth) ? c.maxWidth : 0.f;
  if (wrapping == TextWrapping::NoWrap) {
    mw = 0.f;
  }
  Size s = ts.measure(text, font, color, mw, opts);
  if (std::isfinite(c.maxWidth) && c.maxWidth > 0.f) {
    s.width = std::min(s.width, c.maxWidth);
  }
  if (std::isfinite(c.maxHeight) && c.maxHeight > 0.f) {
    s.height = std::min(s.height, c.maxHeight);
  }
  return s;
}

} // namespace flux
