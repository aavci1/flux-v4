#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace flux::layout {

struct GridTrackMetrics {
  std::size_t columns = 1;
  std::size_t rowCount = 0;
  float cellWidth = 0.f;
  float cellHeight = 0.f;
};

struct GridLayoutResult {
  Size containerSize{};
  std::vector<float> rowHeights{};
  std::vector<float> columnWidths{};
  std::vector<Rect> slots{};
};

GridTrackMetrics resolveGridTrackMetrics(std::size_t columns, std::size_t childCount,
                                         float horizontalSpacing, float verticalSpacing,
                                         float assignedWidth, bool hasAssignedWidth,
                                         float assignedHeight, bool hasAssignedHeight);

LayoutConstraints gridChildConstraints(LayoutConstraints constraints, GridTrackMetrics const& metrics);

GridLayoutResult layoutGrid(GridTrackMetrics const& metrics, float horizontalSpacing, float verticalSpacing,
                            float assignedWidth, bool hasAssignedWidth, float assignedHeight,
                            bool hasAssignedHeight, std::span<Size const> childSizes);

} // namespace flux::layout
