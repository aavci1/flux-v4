#include <Flux/UI/Views/Rectangle.hpp>

#include <Flux/UI/MeasureContext.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"

namespace flux {

Size Rectangle::measure(MeasureContext &ctx, LayoutConstraints const &constraints, LayoutHints const &, TextSystem &) const {
    ctx.advanceChildSlot();
    float const width = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
    return {width, 0.f};
}

namespace detail {

ComponentBuildResult buildMeasuredComponent(Rectangle const &, ComponentBuildContext &ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
    (void)existing;
    Size const resolvedRectSize = build::rectSize(build::assignedFrameForLeaf(
        ctx.paddedContentSize(),
        ctx.innerConstraints(),
        ctx.contentAssignedSize(),
        ctx.hasAssignedWidth(),
        ctx.hasAssignedHeight(),
        ctx.modifiers(),
        ctx.hints()
    ));
    Theme const &theme = ctx.theme();
    auto rectNode = std::make_unique<scenegraph::RectNode>(
        build::sizeRect(resolvedRectSize),
        ctx.modifiers() ? build::resolveFillStyle(ctx.modifiers()->fill, theme) : FillStyle::none(),
        ctx.modifiers() ? build::resolveStrokeStyle(ctx.modifiers()->stroke, theme)
                        : StrokeStyle::none(),
        ctx.modifiers() ? ctx.modifiers()->cornerRadius : CornerRadius {},
        ctx.modifiers() ? build::resolveShadowStyle(ctx.modifiers()->shadow, theme)
                        : ShadowStyle::none()
    );

    ComponentBuildResult result {};
    result.node = std::move(rectNode);
    result.geometrySize = resolvedRectSize;
    result.hasGeometrySize = true;
    return result;
}

} // namespace detail

} // namespace flux
