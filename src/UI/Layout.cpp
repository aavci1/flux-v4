#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {

namespace {

/// When the parent assigned a frame (width/height > 0), use it; otherwise use the finite constraint span.
float assignedSpan(float parentSpan, float outerSpan) {
  if (parentSpan > 0.f) {
    return parentSpan;
  }
  if (std::isfinite(outerSpan) && outerSpan > 0.f) {
    return outerSpan;
  }
  return 0.f;
}

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
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);
  float innerW = std::max(0.f, assignedW - 2.f * value.padding);

  LayoutConstraints childCs = outer;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();

  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    sizes.push_back(le.measure(ctx, ch, childCs, ctx.textSystem()));
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  std::size_t const n = value.children.size();
  std::vector<bool> spacer(n, false);
  std::size_t spacerCount = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if (value.children[i].isSpacer()) {
      spacer[i] = true;
      spacerCount++;
    }
  }

  float maxChildW = 0.f;
  for (Size s : sizes) {
    maxChildW = std::max(maxChildW, s.width);
  }
  if (innerW <= 0.f) {
    innerW = maxChildW;
  }

  LayoutConstraints innerForBuild = outer;
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = std::numeric_limits<float>::infinity();

  float contentH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    contentH += sizes[i].height;
  }
  if (n > 1) {
    contentH += static_cast<float>(n - 1) * value.spacing;
  }

  float extra = 0.f;
  if (std::isfinite(assignedH) && assignedH > 0.f && spacerCount > 0) {
    float const avail = std::max(0.f, assignedH - 2.f * value.padding);
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
    // Stretch each row to the column width so children (e.g. HStack + Spacer) receive the full proposed width.
    // Using only sz.width leaves rows at intrinsic width; a narrow header under a wide wrapped body row would
    // not give flex children (spacers) any extra space.
    float const rowW = std::max(sz.width, innerW);
    float const x = hAlignOffset(rowW, innerW, value.hAlign) + value.padding;
    le.setChildFrame(Rect{x, y, rowW, sz.height});
    ctx.pushConstraints(innerForBuild);
    child.build(ctx);
    ctx.popConstraints();
    y += sz.height + value.spacing;
  }

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<VStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine tmp{};
  float const assignedW =
      std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float innerW = std::max(0.f, assignedW - 2.f * value.padding);

  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float sumH = 2.f * value.padding;
  std::size_t n = value.children.size();
  if (n > 1) {
    sumH += static_cast<float>(n - 1) * value.spacing;
  }
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = tmp.measure(ctx, ch, childCs, ts);
    maxW = std::max(maxW, s.width);
    sumH += s.height;
  }
  ctx.popChildIndex();
  float w = maxW + 2.f * value.padding;
  if (std::isfinite(assignedW) && assignedW > 0.f) {
    w = std::max(w, assignedW);
  }
  return {w, sumH};
}

void Element::Model<HStack>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);

  LayoutConstraints childCs = outer;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    sizes.push_back(le.measure(ctx, ch, childCs, ctx.textSystem()));
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

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
  if (std::isfinite(assignedW) && assignedW > 0.f && spacerCount > 0) {
    float const avail = std::max(0.f, assignedW - 2.f * value.padding);
    extra = std::max(0.f, avail - contentW);
  }
  float const perSpacer = (spacerCount > 0) ? extra / static_cast<float>(spacerCount) : 0.f;

  float const rowInnerH = maxH;

  LayoutConstraints innerForBuild = outer;
  innerForBuild.maxWidth = std::numeric_limits<float>::infinity();
  innerForBuild.maxHeight = rowInnerH;

  float x = value.padding;
  for (std::size_t i = 0; i < n; ++i) {
    Size sz = sizes[i];
    if (spacer[i]) {
      sz.width = sz.width + perSpacer;
    }
    float const y = value.padding + vAlignOffset(sz.height, rowInnerH, value.vAlign);
    le.setChildFrame(Rect{x, y, sz.width, sz.height});
    ctx.pushConstraints(innerForBuild);
    value.children[i].build(ctx);
    ctx.popConstraints();
    x += sz.width + value.spacing;
  }

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<HStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                     TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine tmp{};

  LayoutConstraints childCs = constraints;
  childCs.maxWidth = std::numeric_limits<float>::infinity();
  childCs.maxHeight = std::numeric_limits<float>::infinity();

  float sumW = 2.f * value.padding;
  float maxH = 0.f;
  std::size_t n = value.children.size();
  if (n > 1) {
    sumW += static_cast<float>(n - 1) * value.spacing;
  }
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = tmp.measure(ctx, ch, childCs, ts);
    sumW += s.width;
    maxH = std::max(maxH, s.height);
  }
  ctx.popChildIndex();
  return {sumW, maxH + 2.f * value.padding};
}

void Element::Model<ZStack>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = outer;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float maxH = 0.f;
  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = le.measure(ctx, ch, childCs, ctx.textSystem());
    sizes.push_back(s);
    maxW = std::max(maxW, s.width);
    maxH = std::max(maxH, s.height);
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  if (innerW <= 0.f) {
    innerW = maxW;
  }
  if (innerH <= 0.f) {
    innerH = maxH;
  }

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = innerH;

  for (std::size_t i = 0; i < value.children.size(); ++i) {
    Size const sz = sizes[i];
    // All stack children share the same layout box (max of measured sizes). Using each child's
    // intrinsic size alone leaves a small VStack behind a full-window Rect with a narrow frame,
    // so flex layouts (HStack + Spacer) never receive the full proposed width.
    float const childW = std::max(sz.width, innerW);
    float const childH = std::max(sz.height, innerH);
    float const x = hAlignOffset(childW, innerW, value.hAlign);
    float const y = vAlignOffset(childH, innerH, value.vAlign);
    le.setChildFrame(Rect{x, y, childW, childH});
    ctx.pushConstraints(innerForBuild);
    value.children[i].build(ctx);
    ctx.popConstraints();
  }

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<ZStack>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                       TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine tmp{};
  float const assignedW =
      std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH =
      std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = constraints;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  float maxW = 0.f;
  float maxH = 0.f;
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    Size const s = tmp.measure(ctx, ch, childCs, ts);
    maxW = std::max(maxW, s.width);
    maxH = std::max(maxH, s.height);
  }
  ctx.popChildIndex();
  return {maxW, maxH};
}

void Element::Model<Grid>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const innerW = std::max(0.f, assignedW - 2.f * value.padding);
  std::size_t const cols = std::max<std::size_t>(1, value.columns);
  float const cellW =
      innerW > 0.f
          ? std::max(0.f, (innerW - static_cast<float>(cols - 1) * value.hSpacing) / static_cast<float>(cols))
          : 0.f;

  LayoutConstraints childCs = outer;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();

  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    sizes.push_back(le.measure(ctx, ch, childCs, ctx.textSystem()));
  }
  if (StateStore* store = StateStore::current()) {
    store->resetSlotCursors();
  }
  ctx.rewindChildKeyIndex();

  std::size_t const n = value.children.size();
  std::size_t const rowCount = n == 0 ? 0 : (n + cols - 1) / cols;
  std::vector<float> rowH(rowCount, 0.f);
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t const row = i / cols;
    rowH[row] = std::max(rowH[row], sizes[i].height);
  }

  LayoutConstraints innerForBuild = outer;
  innerForBuild.maxHeight = std::numeric_limits<float>::infinity();
  innerForBuild.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();

  float y = value.padding;
  for (std::size_t r = 0; r < rowCount; ++r) {
    float x = value.padding;
    for (std::size_t c = 0; c < cols; ++c) {
      std::size_t const i = r * cols + c;
      if (i >= n) {
        break;
      }
      Element const& child = value.children[i];
      Size const sz = sizes[i];
      if (child.isSpacer()) {
        ctx.advanceChildSlot();
        x += cellW + value.hSpacing;
        continue;
      }
      float const cx = x + hAlignOffset(sz.width, cellW, value.hAlign);
      float const cy = y + vAlignOffset(sz.height, rowH[r], value.vAlign);
      le.setChildFrame(Rect{cx, cy, sz.width, sz.height});
      ctx.pushConstraints(innerForBuild);
      child.build(ctx);
      ctx.popConstraints();
      x += cellW + value.hSpacing;
    }
    y += rowH[r] + value.vSpacing;
  }

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<Grid>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                   TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine tmp{};
  float const assignedW =
      std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const innerW = std::max(0.f, assignedW - 2.f * value.padding);
  std::size_t const cols = std::max<std::size_t>(1, value.columns);
  float const cellW =
      innerW > 0.f
          ? std::max(0.f, (innerW - static_cast<float>(cols - 1) * value.hSpacing) / static_cast<float>(cols))
          : 0.f;

  LayoutConstraints childCs = constraints;
  childCs.maxHeight = std::numeric_limits<float>::infinity();
  childCs.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();

  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    sizes.push_back(tmp.measure(ctx, ch, childCs, ts));
  }
  ctx.popChildIndex();

  std::size_t const n = value.children.size();
  std::size_t const rowCount = n == 0 ? 0 : (n + cols - 1) / cols;
  std::vector<float> rowH(rowCount, 0.f);
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t const row = i / cols;
    rowH[row] = std::max(rowH[row], sizes[i].height);
  }

  float totalH = 2.f * value.padding;
  if (rowCount > 1) {
    totalH += static_cast<float>(rowCount - 1) * value.vSpacing;
  }
  for (float h : rowH) {
    totalH += h;
  }

  float const totalW = innerW > 0.f ? assignedW : 0.f;
  return {totalW, totalH};
}

void Element::Model<Spacer>::build(BuildContext& ctx) const {
  ctx.advanceChildSlot();
}

Size Element::Model<Spacer>::measure(BuildContext& ctx, LayoutConstraints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const m = std::max(0.f, value.minLength);
  return {m, m};
}

} // namespace flux
