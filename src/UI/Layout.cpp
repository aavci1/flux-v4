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

/// Per-row height when the grid has a finite inner height (`innerH > 0`); otherwise 0 (unconstrained).
float gridCellHeight(float innerH, std::size_t rowCount, float vSpacing) {
  if (innerH <= 0.f || rowCount == 0) {
    return 0.f;
  }
  float const gaps = rowCount > 1 ? static_cast<float>(rowCount - 1) * vSpacing : 0.f;
  return std::max(0.f, (innerH - gaps) / static_cast<float>(rowCount));
}

constexpr float kFlexEpsilon = 1e-4f;

void flexGrowAlongMainAxis(std::vector<float>& alloc, std::vector<Element> const& children, float extra) {
  if (extra <= kFlexEpsilon) {
    return;
  }
  float totalGrow = 0.f;
  for (Element const& ch : children) {
    totalGrow += ch.flexGrow();
  }
  if (totalGrow <= kFlexEpsilon) {
    return;
  }
  for (std::size_t i = 0; i < children.size(); ++i) {
    float const g = children[i].flexGrow();
    if (g > 0.f) {
      alloc[i] += extra * (g / totalGrow);
    }
  }
}

/// Reduces main-axis allocations until `sum(alloc) <= targetSum` (or no shrinkable space left).
/// Shrink weights follow CSS flexbox: proportional to `flexShrink * naturalSize`, using a snapshot
/// of each child's natural main size at shrink start (not the current allocation after min clamps).
void flexShrinkAlongMainAxis(std::vector<float>& alloc, std::vector<Element> const& children,
                             float targetSum) {
  std::vector<float> const natural = alloc;
  for (;;) {
    float sumA = 0.f;
    for (float a : alloc) {
      sumA += a;
    }
    float const need = sumA - targetSum;
    if (need <= kFlexEpsilon) {
      break;
    }
    float sumBasis = 0.f;
    for (std::size_t i = 0; i < children.size(); ++i) {
      float const fs = children[i].flexShrink();
      float const mn = children[i].minMainSize();
      if (fs > 0.f && alloc[i] > mn + kFlexEpsilon) {
        sumBasis += fs * natural[i];
      }
    }
    if (sumBasis <= 1e-6f) {
      break;
    }
    float removedPass = 0.f;
    for (std::size_t i = 0; i < children.size(); ++i) {
      float const fs = children[i].flexShrink();
      float const mn = children[i].minMainSize();
      if (fs <= 0.f || alloc[i] <= mn + kFlexEpsilon) {
        continue;
      }
      float const r = need * (fs * natural[i] / sumBasis);
      float const na = alloc[i] - r;
      if (na < mn) {
        removedPass += alloc[i] - mn;
        alloc[i] = mn;
      } else {
        removedPass += r;
        alloc[i] = na;
      }
    }
    if (removedPass < kFlexEpsilon) {
      break;
    }
  }
}

} // namespace

void Element::Model<VStack>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  if (value.clip && assignedW > 0.f && assignedH > 0.f) {
    layer.clip = Rect{0.f, 0.f, assignedW, assignedH};
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

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

  std::vector<float> allocH(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocH[i] = sizes[i].height;
  }

  // Grow and shrink need a finite assigned main-axis size from the parent. If `assignedH` is not
  // set (e.g. nested in an unconstrained-height VStack), we keep natural sizes — same trade-off as
  // measuring with unbounded maxHeight.
  bool const heightConstrained = std::isfinite(assignedH) && assignedH > 0.f;
  if (heightConstrained && n > 0) {
    float const innerH = std::max(0.f, assignedH - 2.f * value.padding);
    float const gaps = n > 1 ? static_cast<float>(n - 1) * value.spacing : 0.f;
    float const targetSum = std::max(0.f, innerH - gaps);
    float sumNat = 0.f;
    for (float h : allocH) {
      sumNat += h;
    }
    float const extra = targetSum - sumNat;
    if (extra > kFlexEpsilon) {
      flexGrowAlongMainAxis(allocH, value.children, extra);
    } else if (extra < -kFlexEpsilon) {
      flexShrinkAlongMainAxis(allocH, value.children, targetSum);
    }
  }

  float y = value.padding;
  for (std::size_t i = 0; i < n; ++i) {
    Element const& child = value.children[i];
    Size sz = sizes[i];
    sz.height = allocH[i];
    // Stretch each row to the column width so children (e.g. HStack + Spacer) receive the full proposed width.
    // Using only sz.width leaves rows at intrinsic width; a narrow header under a wide wrapped body row would
    // not give flex children (spacers) any extra space.
    float const rowW = std::max(sz.width, innerW);
    float const x = hAlignOffset(rowW, innerW, value.hAlign) + value.padding;
    le.setChildFrame(Rect{x, y, rowW, sz.height});
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxHeight = allocH[i];
    childBuild.minHeight = child.minMainSize();
    ctx.pushConstraints(childBuild);
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

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  if (value.clip && assignedW > 0.f && assignedH > 0.f) {
    layer.clip = Rect{0.f, 0.f, assignedW, assignedH};
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

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

  float maxH = 0.f;
  for (std::size_t i = 0; i < n; ++i) {
    maxH = std::max(maxH, sizes[i].height);
  }
  float const rowInnerH = maxH;

  LayoutConstraints innerForBuild = outer;
  innerForBuild.maxWidth = std::numeric_limits<float>::infinity();
  innerForBuild.maxHeight = rowInnerH;

  std::vector<float> allocW(n);
  for (std::size_t i = 0; i < n; ++i) {
    allocW[i] = sizes[i].width;
  }

  // Grow/shrink along the row only when the stack has a finite assigned width from its parent.
  bool const widthConstrained = std::isfinite(assignedW) && assignedW > 0.f;
  if (widthConstrained && n > 0) {
    float const innerW = std::max(0.f, assignedW - 2.f * value.padding);
    float const gaps = n > 1 ? static_cast<float>(n - 1) * value.spacing : 0.f;
    float const targetSum = std::max(0.f, innerW - gaps);
    float sumNat = 0.f;
    for (float w : allocW) {
      sumNat += w;
    }
    float const extra = targetSum - sumNat;
    if (extra > kFlexEpsilon) {
      flexGrowAlongMainAxis(allocW, value.children, extra);
    } else if (extra < -kFlexEpsilon) {
      flexShrinkAlongMainAxis(allocW, value.children, targetSum);
    }
  }

  float x = value.padding;
  for (std::size_t i = 0; i < n; ++i) {
    Size sz = sizes[i];
    sz.width = allocW[i];
    float const y = value.padding + vAlignOffset(sz.height, rowInnerH, value.vAlign);
    le.setChildFrame(Rect{x, y, sz.width, sz.height});
    LayoutConstraints childBuild = innerForBuild;
    childBuild.maxWidth = allocW[i];
    childBuild.minWidth = value.children[i].minMainSize();
    ctx.pushConstraints(childBuild);
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

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  if (value.clip && assignedW > 0.f && assignedH > 0.f) {
    layer.clip = Rect{0.f, 0.f, assignedW, assignedH};
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

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

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);

  LayerNode layer{};
  if (parentFrame.width > 0.f || parentFrame.height > 0.f) {
    layer.transform = Mat3::translate(parentFrame.x, parentFrame.y);
  }
  if (value.clip && assignedW > 0.f && assignedH > 0.f) {
    layer.clip = Rect{0.f, 0.f, assignedW, assignedH};
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

  float const innerW = std::max(0.f, assignedW - 2.f * value.padding);
  float const innerH = std::max(0.f, assignedH - 2.f * value.padding);
  std::size_t const cols = std::max<std::size_t>(1, value.columns);
  std::size_t const n = value.children.size();
  std::size_t const rowCount = n == 0 ? 0 : (n + cols - 1) / cols;
  float const cellW =
      innerW > 0.f
          ? std::max(0.f, (innerW - static_cast<float>(cols - 1) * value.hSpacing) / static_cast<float>(cols))
          : 0.f;
  float const cellH = gridCellHeight(innerH, rowCount, value.vSpacing);

  LayoutConstraints childCs = outer;
  childCs.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();
  childCs.maxHeight =
      cellH > 0.f ? cellH : std::numeric_limits<float>::infinity();

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

  std::vector<float> rowH(rowCount, 0.f);
  if (cellH > 0.f && rowCount > 0) {
    for (std::size_t r = 0; r < rowCount; ++r) {
      rowH[r] = cellH;
    }
  } else {
    for (std::size_t i = 0; i < n; ++i) {
      std::size_t const row = i / cols;
      rowH[row] = std::max(rowH[row], sizes[i].height);
    }
  }

  LayoutConstraints innerForBuild = outer;
  innerForBuild.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();
  innerForBuild.maxHeight =
      cellH > 0.f ? cellH : std::numeric_limits<float>::infinity();

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
      float const frameW = cellW > 0.f ? cellW : sz.width;
      float const frameH = rowH[r] > 0.f ? rowH[r] : sz.height;
      float const cx = x + hAlignOffset(sz.width, frameW, value.hAlign);
      float const cy = y + vAlignOffset(sz.height, frameH, value.vAlign);
      le.setChildFrame(Rect{cx, cy, frameW, frameH});
      ctx.pushConstraints(innerForBuild);
      child.build(ctx);
      ctx.popConstraints();
      x += cellW + value.hSpacing;
    }
    y += rowH[r];
    if (r + 1 < rowCount) {
      y += value.vSpacing;
    }
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
  float const assignedH =
      std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float const innerW = std::max(0.f, assignedW - 2.f * value.padding);
  float const innerH = std::max(0.f, assignedH - 2.f * value.padding);
  std::size_t const cols = std::max<std::size_t>(1, value.columns);
  std::size_t const n = value.children.size();
  std::size_t const rowCount = n == 0 ? 0 : (n + cols - 1) / cols;
  float const cellW =
      innerW > 0.f
          ? std::max(0.f, (innerW - static_cast<float>(cols - 1) * value.hSpacing) / static_cast<float>(cols))
          : 0.f;
  float const cellH = gridCellHeight(innerH, rowCount, value.vSpacing);

  LayoutConstraints childCs = constraints;
  childCs.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();
  childCs.maxHeight =
      cellH > 0.f ? cellH : std::numeric_limits<float>::infinity();

  std::vector<Size> sizes;
  sizes.reserve(value.children.size());
  ctx.pushChildIndex();
  for (Element const& ch : value.children) {
    sizes.push_back(tmp.measure(ctx, ch, childCs, ts));
  }
  ctx.popChildIndex();

  std::vector<float> rowH(rowCount, 0.f);
  if (cellH > 0.f && rowCount > 0) {
    for (std::size_t r = 0; r < rowCount; ++r) {
      rowH[r] = cellH;
    }
  } else {
    for (std::size_t i = 0; i < n; ++i) {
      std::size_t const row = i / cols;
      rowH[row] = std::max(rowH[row], sizes[i].height);
    }
  }

  float totalH;
  if (cellH > 0.f && rowCount > 0) {
    totalH = assignedH;
  } else {
    totalH = 2.f * value.padding;
    if (rowCount > 1) {
      totalH += static_cast<float>(rowCount - 1) * value.vSpacing;
    }
    for (float h : rowH) {
      totalH += h;
    }
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

void Element::Model<OffsetView>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine& le = ctx.layoutEngine();
  Rect const parentFrame = le.childFrame();
  LayoutConstraints const outer = ctx.constraints();

  float const assignedW = assignedSpan(parentFrame.width, outer.maxWidth);
  float const assignedH = assignedSpan(parentFrame.height, outer.maxHeight);
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  // Scroll viewport must match the *proposed* size from the parent (e.g. ZStack's inner box), not
  // the child's laid-out frame. ZStack expands each child to max(intrinsic, inner); OffsetView's
  // intrinsic height is the full content height, so parentFrame.height would equal content height
  // and clampScrollOffset would think maxScroll ≈ 0.
  float viewportW = innerW;
  float viewportH = innerH;
  if (std::isfinite(outer.maxWidth) && outer.maxWidth > 0.f) {
    viewportW = outer.maxWidth;
  }
  if (std::isfinite(outer.maxHeight) && outer.maxHeight > 0.f) {
    viewportH = outer.maxHeight;
  }

  if (value.viewportSize.signal) {
    value.viewportSize = Size{viewportW, viewportH};
  }

  LayoutConstraints childCs = outer;
  switch (value.axis) {
  case ScrollAxis::Vertical:
    childCs.maxWidth = viewportW > 0.f ? viewportW : std::numeric_limits<float>::infinity();
    childCs.maxHeight = std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Horizontal:
    childCs.maxWidth = std::numeric_limits<float>::infinity();
    childCs.maxHeight = viewportH > 0.f ? viewportH : std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Both:
    childCs.maxWidth = std::numeric_limits<float>::infinity();
    childCs.maxHeight = std::numeric_limits<float>::infinity();
    break;
  }

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
  float totalW = 0.f;
  float totalH = 0.f;

  if (value.axis == ScrollAxis::Horizontal) {
    for (Size s : sizes) {
      totalW += s.width;
      totalH = std::max(totalH, s.height);
    }
  } else {
    for (Size s : sizes) {
      totalW = std::max(totalW, s.width);
      totalH += s.height;
    }
  }

  if (value.contentSize.signal) {
    value.contentSize = Size{totalW, totalH};
  }

  LayerNode layer{};
  float const ox = parentFrame.x - value.offset.x;
  float const oy = parentFrame.y - value.offset.y;
  if (parentFrame.width > 0.f || parentFrame.height > 0.f || value.offset.x != 0.f || value.offset.y != 0.f) {
    layer.transform = Mat3::translate(ox, oy);
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  ctx.pushLayer(layerId);

  if (value.axis == ScrollAxis::Horizontal) {
    float x = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      Size const sz = sizes[i];
      float const rowH = std::max(sz.height, innerH);
      le.setChildFrame(Rect{x, 0.f, sz.width, rowH});
      LayoutConstraints childBuild = outer;
      childBuild.maxWidth = sz.width;
      childBuild.maxHeight = rowH;
      ctx.pushConstraints(childBuild);
      value.children[i].build(ctx);
      ctx.popConstraints();
      x += sz.width;
    }
  } else {
    float y = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      Size const sz = sizes[i];
      float const rowW = std::max(sz.width, innerW);
      le.setChildFrame(Rect{0.f, y, rowW, sz.height});
      LayoutConstraints childBuild = outer;
      childBuild.maxWidth = rowW;
      childBuild.maxHeight = sz.height;
      ctx.pushConstraints(childBuild);
      value.children[i].build(ctx);
      ctx.popConstraints();
      y += sz.height;
    }
  }

  ctx.popChildIndex();
  ctx.popLayer();
}

Size Element::Model<OffsetView>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                           TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  LayoutEngine tmp{};
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = constraints;
  switch (value.axis) {
  case ScrollAxis::Vertical:
    childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
    childCs.maxHeight = std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Horizontal:
    childCs.maxWidth = std::numeric_limits<float>::infinity();
    childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Both:
    childCs.maxWidth = std::numeric_limits<float>::infinity();
    childCs.maxHeight = std::numeric_limits<float>::infinity();
    break;
  }

  float totalW = 0.f;
  float totalH = 0.f;
  ctx.pushChildIndex();
  if (value.axis == ScrollAxis::Horizontal) {
    for (Element const& ch : value.children) {
      Size const s = tmp.measure(ctx, ch, childCs, ts);
      totalW += s.width;
      totalH = std::max(totalH, s.height);
    }
  } else {
    for (Element const& ch : value.children) {
      Size const s = tmp.measure(ctx, ch, childCs, ts);
      totalW = std::max(totalW, s.width);
      totalH += s.height;
    }
  }
  ctx.popChildIndex();
  return {totalW, totalH};
}

void Element::Model<ScrollView>::build(BuildContext& ctx) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  ComponentKey const key = ctx.nextCompositeKey();
  StateStore* store = StateStore::current();
  if (store) {
    store->pushComponent(key);
  }
  Element child{value.body()};
  if (store) {
    store->popComponent();
  }
  ctx.beginCompositeBodySubtree();
  child.build(ctx);
}

Size Element::Model<ScrollView>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                         TextSystem& ts) const {
  if (!ctx.consumeCompositeBodySubtreeRootSkip()) {
    ctx.advanceChildSlot();
  }
  ComponentKey const key = ctx.nextCompositeKey();
  StateStore* store = StateStore::current();
  if (store) {
    store->pushComponent(key);
  }
  Element child{value.body()};
  if (store) {
    store->popComponent();
  }
  ctx.beginCompositeBodySubtree();
  (void)child.measure(ctx, constraints, ts);
  float const w = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const h = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  return {w, h};
}

} // namespace flux
