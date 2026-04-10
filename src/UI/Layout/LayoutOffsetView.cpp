#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/RenderContext.hpp>
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

void OffsetView::layout(LayoutContext& ctx) const {
  ContainerLayoutScope scope(ctx);
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  // Visible scroll viewport: use the parent's layout constraint (scroll slot), not only the assigned
  // frame. ZStack gives a content-sized frame when content is shorter/narrower than the slot, but
  // scroll clamping, hit-testing, and scroll indicators need the full slot size.
  float viewportW = innerW;
  float viewportH = innerH;
  if (std::isfinite(scope.outer.maxWidth) && scope.outer.maxWidth > 0.f) {
    viewportW = scope.outer.maxWidth;
  }
  if (std::isfinite(scope.outer.maxHeight) && scope.outer.maxHeight > 0.f) {
    viewportH = scope.outer.maxHeight;
  }

  if (viewportSize.signal) {
    viewportSize = Size{viewportW, viewportH};
  }

  LayoutConstraints childCs = scope.outer;
  switch (axis) {
  case ScrollAxis::Vertical:
    childCs.maxWidth = viewportW > 0.f ? viewportW : std::numeric_limits<float>::infinity();
    childCs.maxHeight = std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Horizontal:
    childCs.maxWidth = std::numeric_limits<float>::infinity();
    childCs.maxHeight = viewportH > 0.f ? viewportH : std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Both:
    childCs.maxWidth = std::numeric_limits<float>::infinity();
    childCs.maxHeight = std::numeric_limits<float>::infinity();
    break;
  }

  auto sizes = scope.measureChildren(children, childCs);
  scope.logContainer("OffsetView");

  std::size_t const n = children.size();
  float totalW = 0.f;
  float totalH = 0.f;

  if (axis == ScrollAxis::Horizontal) {
    for (Size s : sizes) {
      totalW += s.width;
      totalH = std::max(totalH, s.height);
    }
  } else {
    for (Size s : sizes) {
      totalW = std::max(totalW, s.width);
      totalH += s.height;
    }
  }

  if (contentSize.signal) {
    contentSize = Size{totalW, totalH};
  }

  scope.pushOffsetScrollLayer(offset);

  if (axis == ScrollAxis::Horizontal) {
    float x = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      Size const sz = sizes[i];
      float const rowH = (viewportH > 0.f && std::isfinite(viewportH)) ? viewportH
                                                                       : std::max(sz.height, innerH);
      LayoutConstraints childBuild = scope.outer;
      childBuild.maxWidth = sz.width;
      childBuild.maxHeight = rowH;
      scope.layoutChild(children[i], Rect{x, 0.f, sz.width, rowH}, childBuild);
      x += sz.width;
    }
  } else {
    float y = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      Size const sz = sizes[i];
      float const rowW = (viewportW > 0.f && std::isfinite(viewportW)) ? viewportW
                                                                       : std::max(sz.width, innerW);
      LayoutConstraints childBuild = scope.outer;
      childBuild.maxWidth = rowW;
      childBuild.maxHeight = sz.height;
      scope.layoutChild(children[i], Rect{0.f, y, rowW, sz.height}, childBuild);
      y += sz.height;
    }
  }
}

void OffsetView::renderFromLayout(RenderContext&, LayoutNode const&) const {}

Size OffsetView::measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                         TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  float const viewportW = innerW;
  float const viewportH = innerH;

  LayoutConstraints childCs = constraints;
  switch (axis) {
  case ScrollAxis::Vertical:
    childCs.maxWidth = viewportW > 0.f ? viewportW : std::numeric_limits<float>::infinity();
    childCs.maxHeight = std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Horizontal:
    childCs.maxWidth = std::numeric_limits<float>::infinity();
    childCs.maxHeight = viewportH > 0.f ? viewportH : std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Both:
    childCs.maxWidth = std::numeric_limits<float>::infinity();
    childCs.maxHeight = std::numeric_limits<float>::infinity();
    break;
  }

  float totalW = 0.f;
  float totalH = 0.f;
  if (axis == ScrollAxis::Horizontal) {
    for (Element const& ch : children) {
      Size const s = ch.measure(ctx, childCs, LayoutHints{}, ts);
      totalW += s.width;
      totalH = std::max(totalH, s.height);
    }
  } else {
    for (Element const& ch : children) {
      Size const s = ch.measure(ctx, childCs, LayoutHints{}, ts);
      totalW = std::max(totalW, s.width);
      totalH += s.height;
    }
  }

  return {totalW, totalH};
}

} // namespace flux
