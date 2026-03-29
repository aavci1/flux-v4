#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/StateStore.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

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

  // Match `OffsetView::build`: scrollport size comes from the parent's proposed constraints, not
  // from an expanded child frame (see build pass comment).
  float viewportW = innerW;
  float viewportH = innerH;
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    viewportW = constraints.maxWidth;
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    viewportH = constraints.maxHeight;
  }

  LayoutConstraints childCs = constraints;
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

  if (value.viewportSize.signal) {
    value.viewportSize = Size{viewportW, viewportH};
  }
  if (value.contentSize.signal) {
    value.contentSize = Size{totalW, totalH};
  }

  return {totalW, totalH};
}

} // namespace flux
