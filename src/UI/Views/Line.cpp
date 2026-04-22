#include <Flux/UI/Views/Line.hpp>

#include <Flux/UI/MeasureContext.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"

#include <algorithm>

namespace flux {

Size Line::measure(MeasureContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  float const minX = std::min(from.x, to.x);
  float const maxX = std::max(from.x, to.x);
  float const minY = std::min(from.y, to.y);
  float const maxY = std::max(from.y, to.y);
  return {maxX - minX, maxY - minY};
}

namespace detail {

ComponentBuildResult buildMeasuredComponent(Line const& line, ComponentBuildContext& ctx,
                                            std::unique_ptr<SceneNode> existing) {
  std::unique_ptr<LineSceneNode> lineNode = build::releaseAs<LineSceneNode>(std::move(existing));
  if (!lineNode) {
    lineNode = std::make_unique<LineSceneNode>(ctx.nodeId());
  }
  Theme const& theme = ctx.theme();
  bool dirty = false;
  dirty |= build::updateIfChanged(lineNode->from, line.from);
  dirty |= build::updateIfChanged(lineNode->to, line.to);
  dirty |= build::updateIfChanged(lineNode->stroke, build::resolveStrokeStyle(line.stroke, theme));
  if (dirty) {
    lineNode->invalidatePaint();
    lineNode->markBoundsDirty();
  }
  lineNode->position = {};
  lineNode->recomputeBounds();

  ComponentBuildResult result{};
  result.node = std::move(lineNode);
  result.geometrySize = build::rectSize(lineNode->bounds);
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
