#include <Flux/UI/Views/Grid.hpp>

#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/MountContext.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace flux {

namespace {

std::size_t clampedColumns(std::size_t columns) {
  return std::max<std::size_t>(1, columns);
}

std::size_t spanFor(Grid const& grid, std::size_t index) {
  if (index < grid.columnSpans.size()) {
    return std::max<std::size_t>(1, std::min(grid.columnSpans[index], clampedColumns(grid.columns)));
  }
  return 1;
}

LayoutConstraints fixedConstraints(Size size) {
  return LayoutConstraints{
      .maxWidth = std::max(0.f, size.width),
      .maxHeight = std::max(0.f, size.height),
      .minWidth = std::max(0.f, size.width),
      .minHeight = std::max(0.f, size.height),
  };
}

Size measureChild(Element const& child, MeasureContext& ctx, LayoutConstraints const& constraints,
                  TextSystem& textSystem) {
  ctx.pushConstraints(constraints, LayoutHints{});
  Size size = child.measure(ctx, constraints, LayoutHints{}, textSystem);
  ctx.popConstraints();
  return size;
}

float alignedOffset(float cellExtent, float childExtent, Alignment alignment) {
  switch (alignment) {
  case Alignment::Start:
  case Alignment::Stretch:
    return 0.f;
  case Alignment::Center:
    return std::max(0.f, (cellExtent - childExtent) * 0.5f);
  case Alignment::End:
    return std::max(0.f, cellExtent - childExtent);
  }
  return 0.f;
}

struct GridPlan {
  Size size{};
  std::vector<Rect> slots;
};

GridPlan planGrid(Grid const& grid, LayoutConstraints const& constraints,
                  std::vector<Size> const& measured) {
  GridPlan plan{};
  std::size_t const columns = clampedColumns(grid.columns);
  float const availableWidth = std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f
                                   ? constraints.maxWidth
                                   : 0.f;
  float const totalGap = static_cast<float>(columns - 1) * grid.horizontalSpacing;
  float const columnWidth = columns > 0 ? std::max(0.f, (availableWidth - totalGap) /
                                                            static_cast<float>(columns))
                                        : 0.f;

  plan.slots.resize(measured.size());
  std::size_t column = 0;
  float y = 0.f;
  float rowHeight = 0.f;
  std::size_t rowStart = 0;

  auto finishRow = [&](std::size_t rowEnd) {
    for (std::size_t i = rowStart; i < rowEnd; ++i) {
      plan.slots[i].height = rowHeight;
    }
    y += rowHeight + grid.verticalSpacing;
    rowHeight = 0.f;
    rowStart = rowEnd;
    column = 0;
  };

  for (std::size_t i = 0; i < measured.size(); ++i) {
    std::size_t const span = spanFor(grid, i);
    if (column > 0 && column + span > columns) {
      finishRow(i);
    }
    float const x = static_cast<float>(column) * (columnWidth + grid.horizontalSpacing);
    float const width = columnWidth * static_cast<float>(span) +
                        grid.horizontalSpacing * static_cast<float>(span - 1);
    rowHeight = std::max(rowHeight, measured[i].height);
    plan.slots[i] = Rect{x, y, width, measured[i].height};
    column += span;
    if (column >= columns) {
      finishRow(i + 1);
    }
  }
  if (rowStart < measured.size()) {
    finishRow(measured.size());
  }
  if (!measured.empty()) {
    y -= grid.verticalSpacing;
  }
  plan.size = Size{std::max(availableWidth, constraints.minWidth), std::max(y, constraints.minHeight)};
  return plan;
}

} // namespace

Size Grid::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                   LayoutHints const&, TextSystem& textSystem) const {
  std::size_t const columnsCount = clampedColumns(columns);
  float const availableWidth = std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f
                                   ? constraints.maxWidth
                                   : 0.f;
  float const totalGap = static_cast<float>(columnsCount - 1) * horizontalSpacing;
  float const columnWidth = std::max(0.f, (availableWidth - totalGap) /
                                             static_cast<float>(columnsCount));
  std::vector<Size> measured;
  measured.reserve(children.size());
  for (std::size_t i = 0; i < children.size(); ++i) {
    std::size_t const span = spanFor(*this, i);
    float const width = columnWidth * static_cast<float>(span) +
                        horizontalSpacing * static_cast<float>(span - 1);
    LayoutConstraints childConstraints{
        .maxWidth = width,
        .maxHeight = std::numeric_limits<float>::infinity(),
        .minWidth = 0.f,
        .minHeight = 0.f,
    };
    measured.push_back(measureChild(children[i], ctx, childConstraints, textSystem));
  }
  return planGrid(*this, constraints, measured).size;
}

std::unique_ptr<scenegraph::SceneNode> Grid::mount(MountContext& ctx) const {
  std::size_t const columnsCount = clampedColumns(columns);
  float const availableWidth = std::isfinite(ctx.constraints().maxWidth) && ctx.constraints().maxWidth > 0.f
                                   ? ctx.constraints().maxWidth
                                   : 0.f;
  float const totalGap = static_cast<float>(columnsCount - 1) * horizontalSpacing;
  float const columnWidth = std::max(0.f, (availableWidth - totalGap) /
                                             static_cast<float>(columnsCount));

  std::vector<Size> measured;
  measured.reserve(children.size());
  for (std::size_t i = 0; i < children.size(); ++i) {
    std::size_t const span = spanFor(*this, i);
    float const width = columnWidth * static_cast<float>(span) +
                        horizontalSpacing * static_cast<float>(span - 1);
    LayoutConstraints childConstraints{
        .maxWidth = width,
        .maxHeight = std::numeric_limits<float>::infinity(),
        .minWidth = 0.f,
        .minHeight = 0.f,
    };
    measured.push_back(measureChild(children[i], ctx.measureContext(), childConstraints, ctx.textSystem()));
  }

  GridPlan const plan = planGrid(*this, ctx.constraints(), measured);
  auto group = std::make_unique<scenegraph::GroupNode>(Rect{0.f, 0.f, plan.size.width, plan.size.height});
  for (std::size_t i = 0; i < children.size(); ++i) {
    Rect const slot = plan.slots[i];
    Size childSize = measured[i];
    if (horizontalAlignment == Alignment::Stretch) {
      childSize.width = slot.width;
    }
    if (verticalAlignment == Alignment::Stretch) {
      childSize.height = slot.height;
    }
    MountContext childCtx = ctx.child(fixedConstraints(childSize), {});
    auto childNode = children[i].mount(childCtx);
    if (childNode) {
      childNode->setPosition(Point{
          slot.x + alignedOffset(slot.width, childSize.width, horizontalAlignment),
          slot.y + alignedOffset(slot.height, childSize.height, verticalAlignment),
      });
      group->appendChild(std::move(childNode));
    }
  }
  return group;
}

} // namespace flux
