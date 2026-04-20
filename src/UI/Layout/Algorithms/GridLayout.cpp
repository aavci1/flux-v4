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

std::size_t clampGridSpan(std::size_t span, std::size_t columns) {
  return std::clamp<std::size_t>(span, 1, std::max<std::size_t>(1, columns));
}

std::vector<GridTrackMetrics::Placement> resolvePlacements(std::size_t columns, std::span<std::size_t const> spans) {
  columns = std::max<std::size_t>(1, columns);
  std::vector<GridTrackMetrics::Placement> placements{};
  placements.reserve(spans.size());

  std::size_t row = 0;
  std::size_t column = 0;
  for (std::size_t rawSpan : spans) {
    std::size_t const columnSpan = clampGridSpan(rawSpan, columns);
    if (column > 0 && column + columnSpan > columns) {
      ++row;
      column = 0;
    }
    placements.push_back(GridTrackMetrics::Placement{
        .row = row,
        .column = column,
        .columnSpan = columnSpan,
    });
    column += columnSpan;
    if (column >= columns) {
      ++row;
      column = 0;
    }
  }

  return placements;
}

GridTrackMetrics resolveGridTrackMetricsImpl(std::size_t columns, std::span<std::size_t const> columnSpans,
                                             float horizontalSpacing, float verticalSpacing,
                                             float assignedWidth, bool hasAssignedWidth,
                                             float assignedHeight, bool hasAssignedHeight) {
  (void)verticalSpacing;
  (void)assignedHeight;
  (void)hasAssignedHeight;
  GridTrackMetrics metrics{};
  metrics.columns = std::max<std::size_t>(1, columns);
  metrics.horizontalSpacing = horizontalSpacing;
  metrics.placements = resolvePlacements(metrics.columns, columnSpans);
  metrics.rowCount = metrics.placements.empty() ? 0 : (metrics.placements.back().row + 1);
  metrics.cellWidth = hasAssignedWidth ? gridCellSize(assignedWidth, metrics.columns, horizontalSpacing) : 0.f;
  // Grid rows are content-sized. Even when a parent assigns an outer height to the grid,
  // row tracks should still expand to fit the tallest child in each row instead of being
  // flattened into equal slices of the container height.
  metrics.cellHeight = 0.f;
  return metrics;
}

} // namespace

GridTrackMetrics resolveGridTrackMetrics(std::size_t columns, std::size_t childCount,
                                         float horizontalSpacing, float verticalSpacing,
                                         float assignedWidth, bool hasAssignedWidth,
                                         float assignedHeight, bool hasAssignedHeight) {
  std::vector<std::size_t> spans(childCount, 1u);
  return resolveGridTrackMetricsImpl(columns, spans, horizontalSpacing, verticalSpacing,
                                     assignedWidth, hasAssignedWidth, assignedHeight, hasAssignedHeight);
}

GridTrackMetrics resolveGridTrackMetrics(std::size_t columns, std::span<std::size_t const> columnSpans,
                                         float horizontalSpacing, float verticalSpacing,
                                         float assignedWidth, bool hasAssignedWidth,
                                         float assignedHeight, bool hasAssignedHeight) {
  return resolveGridTrackMetricsImpl(columns, columnSpans, horizontalSpacing, verticalSpacing,
                                     assignedWidth, hasAssignedWidth, assignedHeight, hasAssignedHeight);
}

LayoutConstraints gridChildConstraints(LayoutConstraints constraints, GridTrackMetrics const& metrics,
                                       std::size_t childIndex) {
  std::size_t const span =
      childIndex < metrics.placements.size() ? metrics.placements[childIndex].columnSpan : 1u;
  float const spanWidth =
      metrics.cellWidth > 0.f
          ? (metrics.cellWidth * static_cast<float>(span) +
             static_cast<float>(span - 1u) * metrics.horizontalSpacing)
          : 0.f;
  constraints.maxWidth = spanWidth > 0.f ? spanWidth : std::numeric_limits<float>::infinity();
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
      if (i < metrics.placements.size()) {
        result.rowHeights[metrics.placements[i].row] =
            std::max(result.rowHeights[metrics.placements[i].row], childSizes[i].height);
      }
    }
  }
  if (metrics.cellWidth <= 0.f) {
    bool const hasMultiColumnSpan = std::any_of(metrics.placements.begin(), metrics.placements.end(), [](auto const& p) {
      return p.columnSpan > 1u;
    });
    if (hasMultiColumnSpan) {
      float equalWidth = 0.f;
      for (std::size_t i = 0; i < childSizes.size() && i < metrics.placements.size(); ++i) {
        std::size_t const span = metrics.placements[i].columnSpan;
        float const normalized = std::max(0.f,
                                          (childSizes[i].width - static_cast<float>(span - 1u) * horizontalSpacing) /
                                              static_cast<float>(span));
        equalWidth = std::max(equalWidth, normalized);
      }
      std::fill(result.columnWidths.begin(), result.columnWidths.end(), equalWidth);
    } else {
      std::fill(result.columnWidths.begin(), result.columnWidths.end(), 0.f);
      for (std::size_t i = 0; i < childSizes.size() && i < metrics.placements.size(); ++i) {
        result.columnWidths[metrics.placements[i].column] =
            std::max(result.columnWidths[metrics.placements[i].column], childSizes[i].width);
      }
    }
  }

  std::vector<float> columnOffsets(metrics.columns, 0.f);
  float x = 0.f;
  for (std::size_t col = 0; col < metrics.columns; ++col) {
    columnOffsets[col] = x;
    x += result.columnWidths[col];
    if (col + 1 < metrics.columns) {
      x += horizontalSpacing;
    }
  }

  std::vector<float> rowOffsets(metrics.rowCount, 0.f);
  float y = 0.f;
  for (std::size_t row = 0; row < metrics.rowCount; ++row) {
    rowOffsets[row] = y;
    y += result.rowHeights[row];
    if (row + 1 < metrics.rowCount) {
      y += verticalSpacing;
    }
  }

  for (std::size_t index = 0; index < childSizes.size() && index < metrics.placements.size(); ++index) {
    GridTrackMetrics::Placement const& placement = metrics.placements[index];
    float frameWidth = 0.f;
    for (std::size_t col = 0; col < placement.columnSpan; ++col) {
      frameWidth += result.columnWidths[placement.column + col];
    }
    if (placement.columnSpan > 1u) {
      frameWidth += static_cast<float>(placement.columnSpan - 1u) * horizontalSpacing;
    }
    float const frameHeight =
        metrics.rowCount > 0 ? result.rowHeights[placement.row] : childSizes[index].height;
    result.slots.push_back(Rect{columnOffsets[placement.column], rowOffsets[placement.row], frameWidth, frameHeight});
  }

  if (hasAssignedWidth) {
    result.containerSize.width = std::max(0.f, assignedWidth);
  } else {
    std::size_t usedColumns = 0;
    for (auto const& placement : metrics.placements) {
      usedColumns = std::max(usedColumns, placement.column + placement.columnSpan);
    }
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
