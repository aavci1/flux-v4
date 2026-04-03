#pragma once

/// \file Flux/UI/Views/ScrollView.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutTree.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/OffsetView.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <optional>
#include <vector>

namespace flux {

/// Clamps \p o so the scrolled content does not overscroll past the viewport for \p axis.
/// Non-scrolling axes are zeroed (horizontal-only keeps `o.y == 0`, vertical-only keeps `o.x == 0`).
Point clampScrollOffset(ScrollAxis axis, Point o, Size const& viewport, Size const& content);

/// Scrollable region: children are laid out in an \ref OffsetView and can be dragged or wheel-scrolled.
struct ScrollView : ViewModifiers<ScrollView> {
  // ── Layout / axis ─────────────────────────────────────────────────────────

  ScrollAxis axis = ScrollAxis::Vertical;
  float flexGrow = 1.f;
  float flexShrink = 0.f;
  float minSize = 0.f;
  std::vector<Element> children;

  /// Custom subtree hook (not the generic \ref CompositeComponent path in \ref Element::Model).
  void layout(LayoutContext&) const;
  void renderFromLayout(RenderContext&, LayoutNode const&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;

  // ── Component protocol ─────────────────────────────────────────────────────

  Element body() const;
};

// `ScrollView` is a composite (`body()`) but uses a dedicated `Element::Model` that matches this
// subtree protocol instead of the default composite `Model<C>` implementation.
template<>
struct Element::Model<ScrollView> final : Element::Concept {
  ScrollView value;
  explicit Model(ScrollView c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<ScrollView>>(value);
  }
  void layout(LayoutContext& ctx) const override { value.layout(ctx); }
  void renderFromLayout(RenderContext& ctx, LayoutNode const& node) const override {
    value.renderFromLayout(ctx, node);
  }
  Size measure(LayoutContext& ctx, LayoutConstraints const& c, LayoutHints const& h, TextSystem& ts) const override {
    return value.measure(ctx, c, h, ts);
  }
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
  bool canMemoizeMeasure() const override { return false; }
};

} // namespace flux
