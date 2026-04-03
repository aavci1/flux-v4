#include <Flux/UI/Element.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/RenderContext.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

namespace flux {
using namespace flux::layout;

void ScaleAroundCenter::layout(LayoutContext& ctx) const {
  ContainerLayoutScope scope(ctx);
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);

  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = scope.outer;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  Size const sz = scope.measureChild(child, childCs);
  scope.logContainer("ScaleAroundCenter");

  if (innerW <= 0.f) {
    innerW = sz.width;
  }
  if (innerH <= 0.f) {
    innerH = sz.height;
  }

  float const cx = innerW * 0.5f;
  float const cy = innerH * 0.5f;

  Mat3 const t = Mat3::translate(scope.parentFrame.x, scope.parentFrame.y) * Mat3::translate(cx, cy) *
                 Mat3::scale(scale) * Mat3::translate(-cx, -cy);
  scope.pushScaleAroundCenterLayer(t);

  float const childW = std::max(sz.width, innerW);
  float const childH = std::max(sz.height, innerH);
  float const x = hAlignOffset(childW, innerW, HorizontalAlignment::Center);
  float const y = vAlignOffset(childH, innerH, VerticalAlignment::Center);

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = innerH;
  scope.layoutChild(child, Rect{x, y, childW, childH}, innerForBuild);
}

void ScaleAroundCenter::renderFromLayout(RenderContext&, LayoutNode const&) const {}

Size ScaleAroundCenter::measure(LayoutContext& ctx, LayoutConstraints const& constraints, LayoutHints const&,
                                TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = constraints;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  return child.measure(ctx, childCs, LayoutHints{}, ts);
}

} // namespace flux
