#include <Flux/UI/Views/PathShape.hpp>

#include <Flux/UI/MeasureContext.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"

namespace flux {

Size PathShape::measure(MeasureContext& ctx, LayoutConstraints const&, LayoutHints const&, TextSystem&) const {
  ctx.advanceChildSlot();
  Rect const bounds = path.getBounds();
  return {bounds.width, bounds.height};
}

namespace detail {

ComponentBuildResult buildMeasuredComponent(PathShape const& path, ComponentBuildContext& ctx,
                                            std::unique_ptr<SceneNode> existing) {
  std::unique_ptr<PathSceneNode> pathNode = build::releaseAs<PathSceneNode>(std::move(existing));
  if (!pathNode) {
    pathNode = std::make_unique<PathSceneNode>(ctx.nodeId());
  }
  Theme const& theme = ctx.theme();
  bool dirty = false;
  dirty |= build::updateIfChanged(pathNode->path, path.path);
  dirty |= build::updateIfChanged(
      pathNode->fill, ctx.modifiers() ? build::resolveFillStyle(ctx.modifiers()->fill, theme) : FillStyle::none());
  dirty |= build::updateIfChanged(pathNode->stroke,
                                  ctx.modifiers() ? build::resolveStrokeStyle(ctx.modifiers()->stroke, theme)
                                                  : StrokeStyle::none());
  dirty |= build::updateIfChanged(pathNode->shadow,
                                  ctx.modifiers() ? build::resolveShadowStyle(ctx.modifiers()->shadow, theme)
                                                  : ShadowStyle::none());
  if (dirty) {
    pathNode->invalidatePaint();
    pathNode->markBoundsDirty();
  }
  pathNode->position = {};
  pathNode->recomputeBounds();
  Size const geometrySize = build::rectSize(pathNode->bounds);

  ComponentBuildResult result{};
  result.node = std::move(pathNode);
  result.geometrySize = geometrySize;
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
