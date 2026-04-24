#include <Flux/UI/Views/Text.hpp>

#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/TextNode.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Views/TextSupport.hpp>

#include "UI/Build/ComponentBuildContext.hpp"
#include "UI/Build/ComponentBuildSupport.hpp"

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
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  (void)existing;
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

    auto group = std::make_unique<scenegraph::GroupNode>(
        Rect {0.f, 0.f, frameRect.width, frameRect.height}
    );

    std::vector<std::unique_ptr<scenegraph::SceneNode>> nextChildren{};
    if (selectableState->selection.hasSelection()) {
      std::vector<Rect> const selectionRectsData =
          selectionRects(selectableState->layoutResult, selectableState->selection,
                         &selectableState->text, 0.f, 0.f);
      nextChildren.reserve(selectionRectsData.size() + 1);
      for (std::size_t i = 0; i < selectionRectsData.size(); ++i) {
        Rect const rect = selectionRectsData[i];
        auto rectNode = std::make_unique<scenegraph::RectNode>(
            Rect {rect.x, rect.y, rect.width, rect.height},
            FillStyle::solid(resolvedSelectionColor)
        );
        nextChildren.push_back(std::move(rectNode));
      }
    }

    auto textNode =
        std::make_unique<scenegraph::TextNode>(Rect {0.f, 0.f, frameRect.width, frameRect.height});
    build::configureTextNode(*textNode, frameRect, textLayout);
    nextChildren.push_back(std::move(textNode));

    group->replaceChildren(std::move(nextChildren));
    if (auto interaction = ctx.makeSelectableTextInteraction(selectableState)) {
      group->setInteraction(std::move(interaction));
    }
    result.node = std::move(group);
  } else {
    std::unique_ptr<scenegraph::TextNode> textNode{};
    if (existing && existing->kind() == scenegraph::SceneNodeKind::Text) {
      textNode = std::unique_ptr<scenegraph::TextNode>(
          static_cast<scenegraph::TextNode*>(existing.release()));
      textNode->setBounds(Rect {0.f, 0.f, frameRect.width, frameRect.height});
    } else {
      textNode =
          std::make_unique<scenegraph::TextNode>(Rect {0.f, 0.f, frameRect.width, frameRect.height});
    }
    build::configureTextNode(*textNode, frameRect, textLayout);
    result.node = std::move(textNode);
  }
  result.geometrySize = ctx.layoutOuterSize();
  if (build::textUsesContentBox(ctx.modifiers())) {
    result.geometrySize =
        build::measuredOuterSizeFromContent(build::rectSize(frameRect), ctx.modifiers());
  }
  result.hasGeometrySize = true;
  return result;
}

} // namespace detail

} // namespace flux
