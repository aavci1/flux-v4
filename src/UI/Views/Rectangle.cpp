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

ComponentBuildResult buildMeasuredComponent(Rectangle const &, ComponentBuildContext &ctx, std::unique_ptr<SceneNode> existing) {
    std::unique_ptr<RectSceneNode> rectNode = build::releaseAs<RectSceneNode>(std::move(existing));

    if (!rectNode) {
        rectNode = std::make_unique<RectSceneNode>(ctx.nodeId());
    }

    Size const resolvedRectSize = build::rectSize(build::assignedFrameForLeaf(ctx.paddedContentSize(), ctx.innerConstraints(), ctx.contentAssignedSize(), ctx.hasAssignedWidth(), ctx.hasAssignedHeight(), ctx.modifiers(), ctx.hints()));
    Theme const &theme = ctx.theme();
    bool dirty = false;
    dirty |= build::updateIfChanged(rectNode->size, resolvedRectSize);
    dirty |= build::updateIfChanged(rectNode->cornerRadius, ctx.modifiers() ? ctx.modifiers()->cornerRadius : CornerRadius {});
    dirty |= build::updateIfChanged(rectNode->fill, ctx.modifiers() ? build::resolveFillStyle(ctx.modifiers()->fill, theme) : FillStyle::none());
    dirty |= build::updateIfChanged(rectNode->stroke, ctx.modifiers() ? build::resolveStrokeStyle(ctx.modifiers()->stroke, theme) : StrokeStyle::none());
    dirty |= build::updateIfChanged(rectNode->shadow, ctx.modifiers() ? build::resolveShadowStyle(ctx.modifiers()->shadow, theme) : ShadowStyle::none());

    if (dirty) {
        rectNode->invalidatePaint();
        rectNode->markBoundsDirty();
    }
    rectNode->position = {};
    rectNode->recomputeBounds();

    ComponentBuildResult result {};
    result.node = std::move(rectNode);
    result.geometrySize = resolvedRectSize;
    result.hasGeometrySize = true;
    return result;
}

} // namespace detail

} // namespace flux
