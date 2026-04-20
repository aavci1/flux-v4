#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/Grid.hpp>

#include "UI/Layout/Algorithms/GridLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

Size Grid::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                   TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  std::size_t const n = children.size();
  std::vector<std::size_t> spans(n, 1u);
  for (std::size_t i = 0; i < n && i < columnSpans.size(); ++i) {
    spans[i] = columnSpans[i];
  }
  GridTrackMetrics const metrics =
      resolveGridTrackMetrics(columns, spans, horizontalSpacing, verticalSpacing, assignedW,
                              assignedW > 0.f, assignedH, assignedH > 0.f);

  std::vector<Size> sizes;
  sizes.reserve(children.size());
  for (std::size_t i = 0; i < children.size(); ++i) {
    LayoutConstraints const childCs = gridChildConstraints(constraints, metrics, i);
    sizes.push_back(children[i].measure(ctx, childCs, LayoutHints{}, ts));
  }

  return layoutGrid(metrics, horizontalSpacing, verticalSpacing, assignedW, assignedW > 0.f,
                    assignedH, assignedH > 0.f, sizes)
      .containerSize;
}

} // namespace flux
