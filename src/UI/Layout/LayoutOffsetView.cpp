#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/OffsetView.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

namespace {

struct OffsetViewport {
  float innerW = 0.f;
  float innerH = 0.f;
  float viewportW = 0.f;
  float viewportH = 0.f;
};

OffsetViewport resolveViewport(float assignedW, float assignedH, LayoutConstraints const& outer) {
  OffsetViewport viewport{
      .innerW = std::max(0.f, assignedW),
      .innerH = std::max(0.f, assignedH),
      .viewportW = std::max(0.f, assignedW),
      .viewportH = std::max(0.f, assignedH),
  };

  if (viewport.viewportW <= 0.f && std::isfinite(outer.maxWidth) && outer.maxWidth > 0.f) {
    viewport.viewportW = outer.maxWidth;
  }
  if (viewport.viewportH <= 0.f && std::isfinite(outer.maxHeight) && outer.maxHeight > 0.f) {
    viewport.viewportH = outer.maxHeight;
  }

  return viewport;
}

LayoutConstraints scrollChildConstraints(ScrollAxis axis, LayoutConstraints constraints, float viewportW,
                                         float viewportH) {
  switch (axis) {
  case ScrollAxis::Vertical:
    constraints.maxWidth = viewportW > 0.f ? viewportW : std::numeric_limits<float>::infinity();
    constraints.maxHeight = std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Horizontal:
    constraints.maxWidth = std::numeric_limits<float>::infinity();
    constraints.maxHeight = viewportH > 0.f ? viewportH : std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Both:
    constraints.maxWidth = std::numeric_limits<float>::infinity();
    constraints.maxHeight = std::numeric_limits<float>::infinity();
    break;
  }
  clampLayoutMinToMax(constraints);
  return constraints;
}

Size scrollContentSize(ScrollAxis axis, std::vector<Size> const& sizes) {
  float totalW = 0.f;
  float totalH = 0.f;

  switch (axis) {
  case ScrollAxis::Horizontal:
    for (Size const s : sizes) {
      totalW += s.width;
      totalH = std::max(totalH, s.height);
    }
    break;
  case ScrollAxis::Vertical:
    for (Size const s : sizes) {
      totalW = std::max(totalW, s.width);
      totalH += s.height;
    }
    break;
  case ScrollAxis::Both:
    for (Size const s : sizes) {
      totalW = std::max(totalW, s.width);
      totalH = std::max(totalH, s.height);
    }
    break;
  }

  return {totalW, totalH};
}

} // namespace

Size OffsetView::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                         TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  OffsetViewport const viewport = resolveViewport(assignedW, assignedH, constraints);
  LayoutConstraints const childCs = scrollChildConstraints(axis, constraints, viewport.viewportW, viewport.viewportH);

  std::vector<Size> sizes;
  sizes.reserve(children.size());
  for (Element const& ch : children) {
    sizes.push_back(ch.measure(ctx, childCs, LayoutHints{}, ts));
  }

  return scrollContentSize(axis, sizes);
}

} // namespace flux
