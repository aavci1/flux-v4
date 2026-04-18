#include <Flux/UI/Element.hpp>
#include <Flux/UI/Layout.hpp>

#include "UI/Layout/Algorithms/ScrollLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"

#include <cmath>

namespace flux {

Size ScrollView::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                         TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  Element contentEl = OffsetView{
      .offset = Point{0.f, 0.f},
      .axis = axis,
      .children = children,
  };
  Size const bodySize = contentEl.measure(ctx, constraints, hints, ts);
  return layout::resolveMeasuredScrollViewSize(axis, bodySize, constraints);
}

} // namespace flux
