#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace flux {

namespace {

float hAlignOffset(float childW, float innerW, HorizontalAlignment a) {
  switch (a) {
  case HorizontalAlignment::Leading:
    return 0.f;
  case HorizontalAlignment::Center:
    return (innerW - childW) * 0.5f;
  case HorizontalAlignment::Trailing:
    return innerW - childW;
  }
  return 0.f;
}

float vAlignOffset(float childH, float innerH, VerticalAlignment a) {
  switch (a) {
  case VerticalAlignment::Top:
  case VerticalAlignment::FirstBaseline:
    return 0.f;
  case VerticalAlignment::Center:
    return (innerH - childH) * 0.5f;
  case VerticalAlignment::Bottom:
    return innerH - childH;
  }
  return 0.f;
}

} // namespace

void Element::Model<VStack>::build(BuildContext& ctx) const {
  LayoutEngine& le = ctx.layoutEngine();
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), LayerNode{});
  ctx.pushLayer(layerId);

  LayoutConstraints const outer = ctx.constraints();
  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  for (Element const& ch : value.children) {
    sizes.push_back(le.measure(ch, outer, ctx.textSystem()));
  }

  std::size_t const n = value.children.size();
  std::vector<bool> spacer(n, false);
  std::size_t spacerCount = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (value.children[i].isSpacer()) {
      spacer[i] = true;
      spacerCount++;
    }
  }

  float innerW =
      std::isfinite(outer.maxWidth) ? std::max(0.f, outer.maxWidth - 2.f * value.padding) : 0.f;
  float maxChildW = 0.f;
  for (Size s : sizes) {
    maxChildW = std::max(maxChildW, s.width);
  }
  if (innerW <= 0.f) {
    innerW = maxChildW;
  }

  float contentH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    contentH += sizes[i].height;
  }
  if (n > 1) {
    contentH += static_cast<float>(n - 1) * value.spacing;
  }

  float extra = 0.f;
  if (std::isfinite(outer.maxHeight) && spacerCount > 0) {
    float const avail = std::max(0.f, outer.maxHeight - 2.f * value.padding);
    extra = std::max(0.f, avail - contentH);
  }
  float const perSpacer = (spacerCount > 0) ? extra / static_cast<float>(spacerCount) : 0.f;

  float y = value.padding;
  for (std::size_t i = 0; i < n; ++i) {
    Element const& child = value.children[i];
    Size sz = sizes[i];
    if (spacer[i]) {
      sz.height = sz.height + perSpacer;
    }
    float const x = value.padding + hAlignOffset(sz.width, innerW, value.hAlign);
    le.setChildFrame(Rect{x, y, sz.width, sz.height});
    child.build(ctx);
    y += sz.height + value.spacing;
  }

  ctx.popLayer();
}

Size Element::Model<VStack>::measure(LayoutConstraints const& constraints, TextSystem& ts) const {
  LayoutEngine tmp{};
  float maxW = 0.f;
  float sumH = 2.f * value.padding;
  std::size_t n = value.children.size();
  if (n > 1) {
    sumH += static_cast<float>(n - 1) * value.spacing;
  }
  for (Element const& ch : value.children) {
    Size const s = tmp.measure(ch, constraints, ts);
    maxW = std::max(maxW, s.width);
    sumH += s.height;
  }
  return {maxW + 2.f * value.padding, sumH};
}

void Element::Model<HStack>::build(BuildContext& ctx) const {
  LayoutEngine& le = ctx.layoutEngine();
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), LayerNode{});
  ctx.pushLayer(layerId);

  LayoutConstraints const outer = ctx.constraints();
  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  for (Element const& ch : value.children) {
    sizes.push_back(le.measure(ch, outer, ctx.textSystem()));
  }

  std::size_t const n = value.children.size();
  std::vector<bool> spacer(n, false);
  std::size_t spacerCount = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (value.children[i].isSpacer()) {
      spacer[i] = true;
      spacerCount++;
    }
  }

  float contentW = 0.f;
  float maxH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    contentW += sizes[i].width;
    maxH = std::max(maxH, sizes[i].height);
  }
  if (n > 1) {
    contentW += static_cast<float>(n - 1) * value.spacing;
  }

  float extra = 0.f;
  if (std::isfinite(outer.maxWidth) && spacerCount > 0) {
    float const avail = std::max(0.f, outer.maxWidth - 2.f * value.padding);
    extra = std::max(0.f, avail - contentW);
  }
  float const perSpacer = (spacerCount > 0) ? extra / static_cast<float>(spacerCount) : 0.f;

  float innerH =
      std::isfinite(outer.maxHeight) ? std::max(0.f, outer.maxHeight - 2.f * value.padding) : maxH;

  float x = value.padding;
  for (std::size_t i = 0; i < n; ++i) {
    Size sz = sizes[i];
    if (spacer[i]) {
      sz.width = sz.width + perSpacer;
    }
    float const y = value.padding + vAlignOffset(sz.height, innerH, value.vAlign);
    le.setChildFrame(Rect{x, y, sz.width, sz.height});
    value.children[i].build(ctx);
    x += sz.width + value.spacing;
  }

  ctx.popLayer();
}

Size Element::Model<HStack>::measure(LayoutConstraints const& constraints, TextSystem& ts) const {
  LayoutEngine tmp{};
  float sumW = 2.f * value.padding;
  float maxH = 0.f;
  std::size_t n = value.children.size();
  if (n > 1) {
    sumW += static_cast<float>(n - 1) * value.spacing;
  }
  for (Element const& ch : value.children) {
    Size const s = tmp.measure(ch, constraints, ts);
    sumW += s.width;
    maxH = std::max(maxH, s.height);
  }
  return {sumW, maxH + 2.f * value.padding};
}

void Element::Model<ZStack>::build(BuildContext& ctx) const {
  LayoutEngine& le = ctx.layoutEngine();
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), LayerNode{});
  ctx.pushLayer(layerId);

  LayoutConstraints const outer = ctx.constraints();
  float maxW = 0.f;
  float maxH = 0.f;
  std::vector<Size> sizes;
  for (Element const& ch : value.children) {
    Size const s = le.measure(ch, outer, ctx.textSystem());
    sizes.push_back(s);
    maxW = std::max(maxW, s.width);
    maxH = std::max(maxH, s.height);
  }

  float innerW = std::isfinite(outer.maxWidth) ? std::max(0.f, outer.maxWidth) : maxW;
  float innerH = std::isfinite(outer.maxHeight) ? std::max(0.f, outer.maxHeight) : maxH;

  for (std::size_t i = 0; i < value.children.size(); ++i) {
    Size const sz = sizes[i];
    float const x = hAlignOffset(sz.width, innerW, value.hAlign);
    float const y = vAlignOffset(sz.height, innerH, value.vAlign);
    le.setChildFrame(Rect{x, y, sz.width, sz.height});
    value.children[i].build(ctx);
  }

  ctx.popLayer();
}

Size Element::Model<ZStack>::measure(LayoutConstraints const& constraints, TextSystem& ts) const {
  LayoutEngine tmp{};
  float maxW = 0.f;
  float maxH = 0.f;
  for (Element const& ch : value.children) {
    Size const s = tmp.measure(ch, constraints, ts);
    maxW = std::max(maxW, s.width);
    maxH = std::max(maxH, s.height);
  }
  return {maxW, maxH};
}

void Element::Model<Spacer>::build(BuildContext& ctx) const {
  (void)ctx;
}

Size Element::Model<Spacer>::measure(LayoutConstraints const&, TextSystem&) const {
  float const m = std::max(0.f, value.minLength);
  return {m, m};
}

} // namespace flux
