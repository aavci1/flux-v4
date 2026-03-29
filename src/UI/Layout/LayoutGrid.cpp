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

} // namespace flux
