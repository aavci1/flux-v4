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
  Size measure(MeasureContext&, LayoutConstraints const&, LayoutHints const&, TextSystem&) const;
  [[nodiscard]] std::uint64_t measureCacheKey() const noexcept { return 0x9145c2db37ae6f40ull; }
};

/// Default flex grow 1 (expand along stack main axis); override with chained `.flex(...)`.
template<>
struct Element::Model<Spacer> final : Element::Concept {
  Spacer value;
  explicit Model(Spacer c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override { return std::make_unique<Element::Model<Spacer>>(value); }
  ElementType elementType() const noexcept override { return ElementType::Spacer; }
  std::type_index modelType() const noexcept override { return std::type_index(typeid(Spacer)); }
  void const* rawValuePtr() const noexcept override { return &value; }
  void layout(LayoutContext& ctx) const override { value.layout(ctx); }
  Size measure(MeasureContext& ctx, LayoutConstraints const& c, LayoutHints const& h,
               TextSystem& ts) const override {
    return value.measure(ctx, c, h, ts);
  }
  float flexGrow() const override { return 1.f; }
  float flexShrink() const override { return 0.f; }
  float minMainSize() const override { return 0.f; }
  bool canMemoizeMeasure() const override { return true; }
};

} // namespace flux
