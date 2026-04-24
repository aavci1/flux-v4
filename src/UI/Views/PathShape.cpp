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
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
  Theme const& theme = ctx.theme();
  auto pathNode = std::make_unique<scenegraph::PathNode>(
      Rect {0.f, 0.f, path.path.getBounds().width, path.path.getBounds().height},
      path.path,
      ctx.modifiers() ? build::resolveFillStyle(ctx.modifiers()->fill, theme) : FillStyle::none(),
      ctx.modifiers() ? build::resolveStrokeStyle(ctx.modifiers()->stroke, theme)
                      : StrokeStyle::none(),
      ctx.modifiers() ? build::resolveShadowStyle(ctx.modifiers()->shadow, theme)
                      : ShadowStyle::none()
  );
  Size const geometrySize = build::rectSize(pathNode->localBounds());

  ComponentBuildResult result{};
  result.node = std::move(pathNode);
  result.geometrySize = geometrySize;
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
