#include "UI/Layout/Algorithms/GridLayout.hpp"

#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <limits>

namespace flux::layout {

namespace {

float gridCellSize(float span, std::size_t count, float gap) {
  if (span <= 0.f || count == 0) {
    return 0.f;
  }
  float const gaps = static_cast<float>(count - 1) * gap;
  return std::max(0.f, (span - gaps) / static_cast<float>(count));
}

} // namespace

GridTrackMetrics resolveGridTrackMetrics(std::size_t columns, std::size_t childCount,
                                         float horizontalSpacing, float verticalSpacing,
                                         float assignedWidth, bool hasAssignedWidth,
                                         float assignedHeight, bool hasAssignedHeight) {
  GridTrackMetrics metrics{};
  metrics.columns = std::max<std::size_t>(1, columns);
  metrics.rowCount = childCount == 0 ? 0 : (childCount + metrics.columns - 1) / metrics.columns;
  metrics.cellWidth = hasAssignedWidth ? gridCellSize(assignedWidth, metrics.columns, horizontalSpacing) : 0.f;
  metrics.cellHeight =
      hasAssignedHeight ? gridCellSize(assignedHeight, metrics.rowCount, verticalSpacing) : 0.f;
  return metrics;
}

LayoutConstraints gridChildConstraints(LayoutConstraints constraints, GridTrackMetrics const& metrics) {
  constraints.maxWidth =
      metrics.cellWidth > 0.f ? metrics.cellWidth : std::numeric_limits<float>::infinity();
  constraints.maxHeight =
      metrics.cellHeight > 0.f ? metrics.cellHeight : std::numeric_limits<float>::infinity();
  clampLayoutMinToMax(constraints);
  return constraints;
}

GridLayoutResult layoutGrid(GridTrackMetrics const& metrics, float horizontalSpacing, float verticalSpacing,
                            float assignedWidth, bool hasAssignedWidth, float assignedHeight,
                            bool hasAssignedHeight, std::span<Size const> childSizes) {
  GridLayoutResult result{};
  result.rowHeights.assign(metrics.rowCount, metrics.cellHeight);
  result.columnWidths.assign(metrics.columns, metrics.cellWidth);
  result.slots.reserve(childSizes.size());

  if (metrics.cellHeight <= 0.f) {
    std::fill(result.rowHeights.begin(), result.rowHeights.end(), 0.f);
    for (std::size_t i = 0; i < childSizes.size(); ++i) {
      result.rowHeights[i / metrics.columns] =
          std::max(result.rowHeights[i / metrics.columns], childSizes[i].height);
    }
  }
  if (metrics.cellWidth <= 0.f) {
    std::fill(result.columnWidths.begin(), result.columnWidths.end(), 0.f);
    for (std::size_t i = 0; i < childSizes.size(); ++i) {
      result.columnWidths[i % metrics.columns] =
          std::max(result.columnWidths[i % metrics.columns], childSizes[i].width);
    }
  }

  float y = 0.f;
  for (std::size_t row = 0; row < metrics.rowCount; ++row) {
    float x = 0.f;
    for (std::size_t col = 0; col < metrics.columns; ++col) {
      std::size_t const index = row * metrics.columns + col;
      if (index >= childSizes.size()) {
        break;
      }

      float const frameWidth =
          metrics.cellWidth > 0.f ? metrics.cellWidth : result.columnWidths[col];
      float const frameHeight =
          metrics.rowCount > 0 ? result.rowHeights[row] : childSizes[index].height;
      result.slots.push_back(Rect{x, y, frameWidth, frameHeight});

      x += frameWidth;
      if (col + 1 < metrics.columns && index + 1 < childSizes.size()) {
        x += horizontalSpacing;
      }
    }

    y += metrics.rowCount > 0 ? result.rowHeights[row] : 0.f;
    if (row + 1 < metrics.rowCount) {
      y += verticalSpacing;
    }
  }

  if (hasAssignedWidth) {
    result.containerSize.width = std::max(0.f, assignedWidth);
  } else {
    std::size_t const usedColumns = std::min(metrics.columns, childSizes.size());
    if (usedColumns > 1) {
      result.containerSize.width += static_cast<float>(usedColumns - 1) * horizontalSpacing;
    }
    for (std::size_t col = 0; col < usedColumns; ++col) {
      result.containerSize.width += result.columnWidths[col];
    }
  }

  if (hasAssignedHeight) {
    result.containerSize.height = std::max(0.f, assignedHeight);
  } else {
    if (metrics.rowCount > 1) {
      result.containerSize.height += static_cast<float>(metrics.rowCount - 1) * verticalSpacing;
    }
    for (float rowHeight : result.rowHeights) {
      result.containerSize.height += rowHeight;
    }
  }

  return result;
}

} // namespace flux::layout
