#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/Grid.hpp>

#include "UI/Layout/Algorithms/GridLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"
#include "UI/SceneBuilder/MeasureLayoutCache.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

Size Grid::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                   TextSystem& ts) const {
  ComponentKey const layoutKey = ctx.currentElementKey();
  Element const* const currentElement = ctx.currentElement();
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

  GridLayoutResult const layoutResult =
      layoutGrid(metrics, horizontalSpacing, verticalSpacing, assignedW, assignedW > 0.f,
                 assignedH, assignedH > 0.f, sizes);
  if (currentElement && ctx.layoutCache()) {
    ctx.layoutCache()->recordGridLayout(
        detail::MeasureLayoutKey{
            .measureId = currentElement->measureId(),
            .componentKey = layoutKey,
            .constraints = constraints,
            .hints = hints,
        },
        layoutResult);
  }
  return layoutResult.containerSize;
}

} // namespace flux
