#pragma once

#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace flux {

namespace detail {

inline float forEachHAlignOffset(float rowW, float innerW, HorizontalAlignment a) {
  switch (a) {
  case HorizontalAlignment::Leading:
    return 0.f;
  case HorizontalAlignment::Center:
    return (innerW - rowW) * 0.5f;
  case HorizontalAlignment::Trailing:
    return innerW - rowW;
  }
  return 0.f;
}

} // namespace detail

/// Transparent element expander for dynamic lists (v1: **positional** keys only).
///
/// Each index `i` is built under a stable key path while it stays at that index; prepends and
/// deletes shift indices and will move state accordingly.
///
/// `ForEach` does not add a layout layer: it lays out items in a **vertical** column and sizes
/// itself as the max child width by sum of child heights (plus `spacing`). Use inside `VStack` /
/// `ScrollView` layouts; other parents may get incorrect intrinsic size.
///
/// The `factory` runs during measure **and** build each frame — keep it free of one-off side
/// effects. For large `T`, prefer storing ids or `shared_ptr` and capturing those in lambdas.
template<typename T>
struct ForEach {
  std::vector<T> items;
  std::function<Element(T const&)> factory;
  float spacing = 0.f;
  HorizontalAlignment hAlign = HorizontalAlignment::Leading;

  ForEach(std::vector<T> itemsIn, std::function<Element(T const&)> factoryIn, float spacingIn = 0.f)
      : items(std::move(itemsIn)), factory(std::move(factoryIn)), spacing(spacingIn) {}
};

template<typename T>
struct Element::Model<ForEach<T>> final : Element::Concept {
  ForEach<T> value;

  explicit Model(ForEach<T> v) : value(std::move(v)) {}

  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<ForEach<T>>>(value);
  }

  void build(BuildContext& ctx) const override;
  Size measure(BuildContext& ctx, LayoutConstraints const& constraints, TextSystem& ts) const override;

  float flexGrow() const override { return 0.f; }
  float flexShrink() const override { return 0.f; }
  float minMainSize() const override { return 0.f; }
};

template<typename T>
void Element::Model<ForEach<T>>::build(BuildContext& ctx) const {
  LayoutEngine& le = ctx.layoutEngine();
  Rect const box = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  ComponentKey const forEachKey = ctx.nextCompositeKey();

  LayoutEngine tmp{};
  LayoutConstraints childCsMeasure = outer;
  childCsMeasure.maxHeight = std::numeric_limits<float>::infinity();
  float const assignedW = std::isfinite(outer.maxWidth) ? outer.maxWidth : 0.f;
  childCsMeasure.maxWidth =
      assignedW > 0.f ? assignedW : std::numeric_limits<float>::infinity();

  std::vector<Size> sizes;
  std::size_t const n = value.items.size();
  sizes.reserve(n);

  for (std::size_t i = 0; i < n; ++i) {
    Element item{value.factory(value.items[i])};
    ctx.pushCompositeKeyTail(forEachKey);
    ctx.setChildIndex(i);
    sizes.push_back(tmp.measure(ctx, item, childCsMeasure, ctx.textSystem()));
    ctx.popCompositeKeyTail();
  }

  float maxChildW = 0.f;
  for (Size s : sizes) {
    maxChildW = std::max(maxChildW, s.width);
  }

  float innerW = box.width > 0.f ? box.width : maxChildW;
  if (innerW <= 0.f) {
    innerW = maxChildW;
  }

  float y = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    if (i > 0) {
      y += value.spacing;
    }
    Element item{value.factory(value.items[i])};
    Size sz = sizes[i];
    float const rowW = std::max(sz.width, innerW);
    float const x = detail::forEachHAlignOffset(rowW, innerW, value.hAlign);
    le.setChildFrame(Rect{x, y, rowW, sz.height});

    LayoutConstraints childBuild = outer;
    childBuild.maxHeight = sz.height;
    childBuild.minHeight = item.minMainSize();
    ctx.pushConstraints(childBuild);

    ctx.pushCompositeKeyTail(forEachKey);
    ctx.setChildIndex(i);
    item.build(ctx);
    ctx.popCompositeKeyTail();

    ctx.popConstraints();
    y += sz.height;
  }
}

template<typename T>
Size Element::Model<ForEach<T>>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                         TextSystem& ts) const {
  ComponentKey const forEachKey = ctx.nextCompositeKey();
  LayoutEngine tmp{};
  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  childCs.maxWidth =
      assignedW > 0.f ? assignedW : std::numeric_limits<float>::infinity();

  float totalW = 0.f;
  float totalH = 0.f;
  std::size_t const n = value.items.size();

  for (std::size_t i = 0; i < n; ++i) {
    Element item{value.factory(value.items[i])};
    ctx.pushCompositeKeyTail(forEachKey);
    ctx.setChildIndex(i);
    Size const s = tmp.measure(ctx, item, childCs, ts);
    ctx.popCompositeKeyTail();
    totalW = std::max(totalW, s.width);
    totalH += s.height;
  }
  if (n > 1 && value.spacing > 0.f) {
    totalH += static_cast<float>(n - 1) * value.spacing;
  }
  return {totalW, totalH};
}

} // namespace flux
