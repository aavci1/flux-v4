#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Views/Grid.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

void Grid::build(BuildContext& ctx) const {
  ContainerBuildScope scope(ctx);
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  scope.pushStandardLayer(false, assignedW, assignedH);

  float const innerW = std::max(0.f, assignedW);
  float const innerH = std::max(0.f, assignedH);
  std::size_t const cols = std::max<std::size_t>(1, columns);
  std::size_t const n = children.size();
  std::size_t const rowCount = n == 0 ? 0 : (n + cols - 1) / cols;
  float const cellW =
      innerW > 0.f
          ? std::max(0.f, (innerW - static_cast<float>(cols - 1) * hSpacing) / static_cast<float>(cols))
          : 0.f;
  float const cellH = gridCellHeight(innerH, rowCount, vSpacing);

  LayoutConstraints childCs = scope.outer;
  childCs.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();
  childCs.maxHeight =
      cellH > 0.f ? cellH : std::numeric_limits<float>::infinity();

  auto sizes = scope.measureChildren(children, childCs);
  scope.logContainer("Grid");

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

  LayoutConstraints innerForBuild = scope.outer;
  innerForBuild.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();
  innerForBuild.maxHeight =
      cellH > 0.f ? cellH : std::numeric_limits<float>::infinity();

  float y = 0.f;
  for (std::size_t r = 0; r < rowCount; ++r) {
    float x = 0.f;
    for (std::size_t c = 0; c < cols; ++c) {
      std::size_t const i = r * cols + c;
      if (i >= n) {
        break;
      }
      Size const sz = sizes[i];
      float const frameW = cellW > 0.f ? cellW : sz.width;
      float const frameH = rowH[r] > 0.f ? rowH[r] : sz.height;
      float const cx = x + hAlignOffset(sz.width, frameW, hAlign);
      float const cy = y + vAlignOffset(sz.height, frameH, vAlign);
      scope.buildChild(children[i], Rect{cx, cy, frameW, frameH}, innerForBuild);
      x += cellW + hSpacing;
    }
    y += rowH[r];
    if (r + 1 < rowCount) {
      y += vSpacing;
    }
  }
}

Size Grid::measure(BuildContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                   TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW =
      std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH =
      std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float const innerW = std::max(0.f, assignedW);
  float const innerH = std::max(0.f, assignedH);
  std::size_t const cols = std::max<std::size_t>(1, columns);
  std::size_t const n = children.size();
  std::size_t const rowCount = n == 0 ? 0 : (n + cols - 1) / cols;
  float const cellW =
      innerW > 0.f
          ? std::max(0.f, (innerW - static_cast<float>(cols - 1) * hSpacing) / static_cast<float>(cols))
          : 0.f;
  float const cellH = gridCellHeight(innerH, rowCount, vSpacing);

  LayoutConstraints childCs = constraints;
  childCs.maxWidth =
      cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();
  childCs.maxHeight =
      cellH > 0.f ? cellH : std::numeric_limits<float>::infinity();

  std::vector<Size> sizes;
  sizes.reserve(children.size());
  for (Element const& ch : children) {
    sizes.push_back(ch.measure(ctx, childCs, LayoutHints{}, ts));
  }

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
    totalH = 0.f;
    if (rowCount > 1) {
      totalH += static_cast<float>(rowCount - 1) * vSpacing;
    }
    for (float h : rowH) {
      totalH += h;
    }
  }

  float const totalW = innerW > 0.f ? assignedW : 0.f;
  return {totalW, totalH};
}

} // namespace flux
