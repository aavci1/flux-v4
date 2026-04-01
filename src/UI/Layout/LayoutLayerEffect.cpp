#include <Flux/UI/Element.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <Flux/Scene/Nodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>

#include "UI/Layout/ContainerScope.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <cmath>
#include <cstddef>
#include <limits>

namespace flux {
using namespace flux::layout;

void Element::Model<LayerEffect>::build(BuildContext& ctx) const {
  ContainerBuildScope scope(ctx);
  float const assignedW = assignedSpan(scope.parentFrame.width, scope.outer.maxWidth);
  float const assignedH = assignedSpan(scope.parentFrame.height, scope.outer.maxHeight);
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = scope.outer;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  Size const sz = scope.measureChild(value.child, childCs);
  scope.logContainer("LayerEffect");

  if (innerW <= 0.f) {
    innerW = sz.width;
  }
  if (innerH <= 0.f) {
    innerH = sz.height;
  }

  LayerNode layer{};
  layer.opacity = value.opacity;
  layer.transform =
      Mat3::translate(scope.parentFrame.x + value.offset.x, scope.parentFrame.y + value.offset.y);
  NodeId const layerId = ctx.graph().addLayer(ctx.parentLayer(), std::move(layer));
  scope.pushCustomLayer(layerId);

  float const childW = std::max(sz.width, innerW);
  float const childH = std::max(sz.height, innerH);
  float const x = hAlignOffset(childW, innerW, HorizontalAlignment::Center);
  float const y = vAlignOffset(childH, innerH, VerticalAlignment::Center);

  LayoutConstraints innerForBuild{};
  innerForBuild.maxWidth = innerW;
  innerForBuild.maxHeight = innerH;
  scope.buildChild(value.child, Rect{x, y, childW, childH}, innerForBuild);
}

Size Element::Model<LayerEffect>::measure(BuildContext& ctx, LayoutConstraints const& constraints,
                                         LayoutHints const&, TextSystem& ts) const {
  ContainerMeasureScope scope(ctx);
  float const assignedW = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  float const assignedH = std::isfinite(constraints.maxHeight) ? constraints.maxHeight : 0.f;
  float innerW = std::max(0.f, assignedW);
  float innerH = std::max(0.f, assignedH);

  LayoutConstraints childCs = constraints;
  childCs.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
  childCs.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();

  return value.child.measure(ctx, childCs, LayoutHints{}, ts);
}

} // namespace flux
