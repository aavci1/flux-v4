#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/OffsetView.hpp>

#include "UI/Layout/Algorithms/ScrollLayout.hpp"
#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

Size OffsetView::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                         TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  Size viewport{
      std::max(0.f, assignedW),
      std::max(0.f, assignedH),
  };
  if (viewport.width <= 0.f && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    viewport.width = constraints.maxWidth;
  }
  if (viewport.height <= 0.f && std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    viewport.height = constraints.maxHeight;
  }

  LayoutConstraints const childCs = layout::scrollChildConstraints(axis, constraints, viewport);

  std::vector<Size> sizes;
  sizes.reserve(children.size());
  for (Element const& ch : children) {
    sizes.push_back(ch.measure(ctx, childCs, LayoutHints{}, ts));
  }

  return layout::scrollContentSize(axis, sizes);
}

} // namespace flux
