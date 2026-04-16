#pragma once

/// \file Flux/UI/Views/Spacer.hpp
///
/// Flexible empty region for HStack/VStack: expands along the stack main axis.
/// Set flex and minimum main size with chained `.flex(grow, shrink, minMain)` (defaults are grow=1).

#include <Flux/UI/Detail/PrimitiveForwards.hpp>
#include <Flux/UI/ViewModifiers.hpp>

#include <cstdint>
#include <memory>

namespace flux {

struct Spacer : ViewModifiers<Spacer> {
  static constexpr bool memoizable = true;

  void layout(LayoutContext&) const;
  void renderFromLayout(RenderContext&, LayoutNode const&) const;
  Size measure(LayoutContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;
  [[nodiscard]] std::uint64_t measureCacheKey() const noexcept { return 0x9145c2db37ae6f40ull; }
};

/// Default flex grow 1 (expand along stack main axis); override with chained `.flex(...)`.
template<>
struct Element::Model<Spacer> final : Element::Concept {
  Spacer value;
  explicit Model(Spacer c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override { return std::make_unique<Element::Model<Spacer>>(value); }
  void layout(LayoutContext& ctx) const override { value.layout(ctx); }
  void renderFromLayout(RenderContext& ctx, LayoutNode const& node) const override {
    value.renderFromLayout(ctx, node);
  }
  Size measure(LayoutContext& ctx, LayoutConstraints const& c, LayoutHints const& h, TextSystem& ts) const override {
    return value.measure(ctx, c, h, ts);
  }
  float flexGrow() const override { return 1.f; }
  float flexShrink() const override { return 0.f; }
  float minMainSize() const override { return 0.f; }
  bool canMemoizeMeasure() const override { return true; }
};

} // namespace flux
