#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace flux {
using namespace flux::layout;

void Element::Model<OffsetView>::build(BuildContext& ctx) const {
  ContainerBuildScope scope(ctx);
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  float viewportW = innerW;
  float viewportH = innerH;
  if (std::isfinite(scope.outer.maxWidth) && scope.outer.maxWidth > 0.f) {
    viewportW = scope.outer.maxWidth;
  }
  if (std::isfinite(scope.outer.maxHeight) && scope.outer.maxHeight > 0.f) {
    viewportH = scope.outer.maxHeight;
  }

  if (value.viewportSize.signal) {
    value.viewportSize = Size{viewportW, viewportH};
  }

  LayoutConstraints childCs = scope.outer;
  switch (value.axis) {
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

  auto sizes = scope.measureChildren(value.children, childCs);

  std::size_t const n = value.children.size();
  float totalW = 0.f;
  float totalH = 0.f;

  if (value.axis == ScrollAxis::Horizontal) {
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

  if (value.contentSize.signal) {
    value.contentSize = Size{totalW, totalH};
  }

  LayerNode layer{};
  float const ox = scope.parentFrame.x - value.offset.x;
  float const oy = scope.parentFrame.y - value.offset.y;
  if (scope.parentFrame.width > 0.f || scope.parentFrame.height > 0.f ||
      value.offset.x != 0.f || value.offset.y != 0.f) {
    layer.transform = Mat3::translate(ox, oy);
  }
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  scope.pushCustomLayer(layerId);

  if (value.axis == ScrollAxis::Horizontal) {
    float x = 0.f;
    for (std::size_t i = 0; i < n; ++i) {
      Size const sz = sizes[i];
      float const rowH = (viewportH > 0.f && std::isfinite(viewportH)) ? viewportH
                                                                       : std::max(sz.height, innerH);
      LayoutConstraints childBuild = scope.outer;
      childBuild.maxWidth = sz.width;
      childBuild.maxHeight = rowH;
      childBuild.vStackCrossAlign = std::nullopt;
      scope.buildChild(value.children[i], Rect{x, 0.f, sz.width, rowH}, childBuild);
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
      childBuild.vStackCrossAlign = std::nullopt;
      scope.buildChild(value.children[i], Rect{0.f, y, rowW, sz.height}, childBuild);
      y += sz.height;
    }
  }
}

Size Element::Model<OffsetView>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                         TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  float viewportW = innerW;
  float viewportH = innerH;
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    viewportW = constraints.maxWidth;
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    viewportH = constraints.maxHeight;
  }

  LayoutConstraints childCs = constraints;
  switch (value.axis) {
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
  if (value.axis == ScrollAxis::Horizontal) {
    for (Element const& ch : value.children) {
      Size const s = ch.measure(ctx, childCs, ts);
      totalW += s.width;
      totalH = std::max(totalH, s.height);
    }
  } else {
    for (Element const& ch : value.children) {
      Size const s = ch.measure(ctx, childCs, ts);
      totalW = std::max(totalW, s.width);
      totalH += s.height;
    }
  }

  return {totalW, totalH};
}

} // namespace flux
