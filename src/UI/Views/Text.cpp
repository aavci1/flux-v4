#include <Flux/UI/Views/Text.hpp>

#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Views/TextSupport.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/SceneBuilder/NodeReuse.hpp"

#include <algorithm>
#include <cmath>

namespace flux {

Size Text::measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
                   TextSystem& textSystem) const {
  ctx.advanceChildSlot();
  (void)hints;
  auto const [resolvedFont, resolvedColor] = text_detail::resolveBodyTextStyle(font, color);
  TextLayoutOptions const options = text_detail::makeTextLayoutOptions(*this);
  float maxWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
  if (wrapping == TextWrapping::NoWrap) {
    maxWidth = 0.f;
  }
  Size size = textSystem.measure(text, resolvedFont, resolvedColor, maxWidth, options);
  if (std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    size.width = std::min(size.width, constraints.maxWidth);
  }
  if (std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.height = std::min(size.height, constraints.maxHeight);
  }
  return size;
}

namespace detail {

ComponentBuildResult buildMeasuredComponent(Text const& text, ComponentBuildContext& ctx,
                                            std::unique_ptr<SceneNode> existing) {
  Theme const& theme = ctx.theme();
  Font const resolvedFont = resolveFont(text.font, theme.bodyFont, theme);
  Color const resolvedColor = resolveColor(text.color, theme.labelColor, theme);
  Color const resolvedSelectionColor =
      resolveColor(text.selectionColor, theme.selectedContentBackgroundColor, theme);
  LayoutConstraints const textFrameConstraints =
      build::textUsesContentBox(ctx.modifiers()) ? ctx.contentBoxConstraints() : ctx.innerConstraints();
  Rect const frameRect =
      build::assignedFrameForLeaf(ctx.paddedContentSize(), textFrameConstraints, ctx.contentAssignedSize(),
                                  ctx.hasAssignedWidth(), ctx.hasAssignedHeight(), ctx.modifiers(), ctx.hints());
  TextLayoutOptions const options = text_detail::makeTextLayoutOptions(text);
  std::string const displayText =
      text.selectable ? text.text
                      : text_detail::ellipsizedPlainText(text.text, resolvedFont, resolvedColor, frameRect, options,
                                                          ctx.textSystem());
  std::shared_ptr<TextLayout const> textLayout =
      ctx.textSystem().layout(displayText, resolvedFont, resolvedColor,
                              Rect{0.f, 0.f, frameRect.width, frameRect.height}, options);
  std::shared_ptr<SelectableTextState> selectableState{};

  ComponentBuildResult result{};
  if (text.selectable && textLayout && text_detail::hasRenderableTextGeometry(*textLayout)) {
    selectableState = selectableTextState(ctx.key());
    updateSelectableTextLayout(*selectableState, textLayout, text.text, frameRect.width);

    std::unique_ptr<SceneNode> group = build::releasePlainGroup(std::move(existing));
    if (!group) {
      group = std::make_unique<SceneNode>(ctx.nodeId());
    }
    ReusableSceneNodes reusable = releaseReusableChildren(*group);

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    if (selectableState->selection.hasSelection()) {
      std::vector<Rect> const selectionRectsData =
          selectionRects(selectableState->layoutResult, selectableState->selection,
                         &selectableState->text, 0.f, 0.f);
      nextChildren.reserve(selectionRectsData.size() + 1);
      for (std::size_t i = 0; i < selectionRectsData.size(); ++i) {
        NodeId const rectId = SceneTree::childId(ctx.nodeId(), LocalId::fromIndex(i));
        std::unique_ptr<RectSceneNode> rectNode = takeReusableNodeAs<RectSceneNode>(reusable, rectId);
        if (!rectNode) {
          rectNode = std::make_unique<RectSceneNode>(rectId);
        }
        Rect const rect = selectionRectsData[i];
        bool dirty = false;
        dirty |= build::updateIfChanged(rectNode->size, Size{rect.width, rect.height});
        dirty |= build::updateIfChanged(rectNode->fill, FillStyle::solid(resolvedSelectionColor));
        dirty |= build::updateIfChanged(rectNode->stroke, StrokeStyle::none());
        dirty |= build::updateIfChanged(rectNode->shadow, ShadowStyle::none());
        dirty |= build::updateIfChanged(rectNode->cornerRadius, CornerRadius{});
        if (dirty) {
          rectNode->invalidatePaint();
          rectNode->markBoundsDirty();
        }
        rectNode->position = Point{rect.x, rect.y};
        rectNode->recomputeBounds();
        nextChildren.push_back(std::move(rectNode));
      }
    }

    NodeId const textId = SceneTree::childId(ctx.nodeId(), LocalId::fromString("$text"));
    std::unique_ptr<TextSceneNode> textNode = takeReusableNodeAs<TextSceneNode>(reusable, textId);
    if (!textNode) {
      textNode = std::make_unique<TextSceneNode>(textId);
    }
    build::configureTextSceneNode(*textNode, ctx.textSystem(), text, resolvedFont, resolvedColor, frameRect,
                                  displayText, textLayout);
    nextChildren.push_back(std::move(textNode));

    group->replaceChildren(std::move(nextChildren));
    build::setGroupBounds(*group, build::rectSize(frameRect));
    result.interaction = ctx.makeSelectableTextInteraction(selectableState);
    result.node = std::move(group);
  } else {
    std::unique_ptr<TextSceneNode> textNode = build::releaseAs<TextSceneNode>(std::move(existing));
    if (!textNode) {
      textNode = std::make_unique<TextSceneNode>(ctx.nodeId());
    }
    build::configureTextSceneNode(*textNode, ctx.textSystem(), text, resolvedFont, resolvedColor, frameRect,
                                  displayText, textLayout);
    result.node = std::move(textNode);
  }
  result.geometrySize = ctx.layoutOuterSize();
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
