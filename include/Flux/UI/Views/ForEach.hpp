#pragma once

/// \file Flux/UI/Views/ForEach.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Detail/LayoutDebugDump.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/LayoutTree.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

namespace flux {

/// Transparent element expander for dynamic lists (v1: **positional** keys only).
///
/// Each index `i` is built under a stable key path while it stays at that index; prepends and
/// deletes shift indices and will move state accordingly.
///
/// `ForEach` adds a layer translated by its assigned frame origin so children are positioned in
/// local coordinates; it lays out items in a **vertical** column and sizes itself as the max child
/// width by sum of child heights (plus `spacing`). Use inside `VStack` / `ScrollView` layouts; other
/// parents may get incorrect intrinsic size.
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

  void layout(LayoutContext& ctx) const override;
  void renderFromLayout(RenderContext&, LayoutNode const&) const override {}
  Size measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& ts) const override;

  float flexGrow() const override { return 0.f; }
  float flexShrink() const override { return 0.f; }
  float minMainSize() const override { return 0.f; }
};

template<typename T>
void Element::Model<ForEach<T>>::layout(LayoutContext& ctx) const {
  LayoutEngine& le = ctx.layoutEngine();
  Rect const box = le.consumeAssignedFrame();
  LayoutConstraints const outer = ctx.constraints();

  ComponentKey const forEachKey = ctx.nextCompositeKey();

  std::size_t const n = value.items.size();

  LayoutConstraints childCsMeasure = outer;
  childCsMeasure.maxHeight = std::numeric_limits<float>::infinity();
  float const assignedW = std::isfinite(outer.maxWidth) ? outer.maxWidth : 0.f;
  childCsMeasure.maxWidth =
      assignedW > 0.f ? assignedW : std::numeric_limits<float>::infinity();

  std::vector<Size> sizes;
  sizes.reserve(n);

  for (std::size_t i = 0; i < n; ++i) {
    Element& item = ctx.pinElement(value.factory(value.items[i]));
    ctx.pushCompositeKeyTail(forEachKey);
    ctx.setChildIndex(i);
    LayoutHints childHints{};
    childHints.vStackCrossAlign = value.hAlign;
    sizes.push_back(item.measure(ctx, childCsMeasure, childHints, ctx.textSystem()));
    ctx.popCompositeKeyTail();
  }

  layoutDebugLogContainer("ForEach", outer, box);

  float maxChildW = 0.f;
  for (Size s : sizes) {
    maxChildW = std::max(maxChildW, s.width);
  }

  float innerW = box.width > 0.f ? box.width : maxChildW;
  if (innerW <= 0.f) {
    innerW = maxChildW;
  }

  ctx.pushLayerWorldTransform(Mat3::translate(box.x, box.y));

  LayoutNode fx{};
  fx.kind = LayoutNode::Kind::Container;
  fx.frame = box;
  fx.constraints = outer;
  fx.hints = ctx.hints();
  fx.containerSpec.kind = ContainerLayerSpec::Kind::Standard;
  fx.element = ctx.currentElement();
  LayoutNodeId const feId = ctx.pushLayoutNode(std::move(fx));
  ctx.pushLayoutParent(feId);

  float y = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    if (i > 0) {
      y += value.spacing;
    }
    Element& item = ctx.pinElement(value.factory(value.items[i]));
    Size sz = sizes[i];
    float const rowW = innerW > 0.f ? innerW : sz.width;
    float const x = 0.f;
    le.setChildFrame(Rect{x, y, rowW, sz.height});

    LayoutConstraints childBuild = outer;
    if (innerW > 0.f) {
      childBuild.maxWidth = innerW;
    }
    childBuild.maxHeight = sz.height;
    childBuild.minHeight = item.minMainSize();
    LayoutHints rowHints{};
    rowHints.vStackCrossAlign = value.hAlign;
    ctx.pushConstraints(childBuild, rowHints);

    ctx.pushCompositeKeyTail(forEachKey);
    ctx.setChildIndex(i);
    item.layout(ctx);
    ctx.popCompositeKeyTail();

    ctx.popConstraints();
    y += sz.height;
  }

  ctx.popLayoutParent();
  ctx.popLayerWorldTransform();
}

template<typename T>
Size Element::Model<ForEach<T>>::measure(LayoutContext& ctx, LayoutConstraints const& constraints,
                                            LayoutHints const&, TextSystem& ts) const {
  ComponentKey const forEachKey = ctx.nextCompositeKey();
  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  childCs.maxWidth =
      assignedW > 0.f ? assignedW : std::numeric_limits<float>::infinity();

  LayoutHints childHints{};
  childHints.vStackCrossAlign = value.hAlign;

  float totalW = 0.f;
  float totalH = 0.f;
  std::size_t const n = value.items.size();

  for (std::size_t i = 0; i < n; ++i) {
    Element item{value.factory(value.items[i])};
    ctx.pushCompositeKeyTail(forEachKey);
    ctx.setChildIndex(i);
    Size const s = item.measure(ctx, childCs, childHints, ts);
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
