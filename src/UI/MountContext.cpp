#include <Flux/UI/MountContext.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/TextNode.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Detail/MountPosition.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <Flux/Reactive/Effect.hpp>

#include "UI/Layout/Algorithms/StackLayout.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace flux {

namespace {

Theme const& activeTheme(EnvironmentStack& environment) {
  if (Theme const* theme = environment.find<Theme>()) {
    return *theme;
  }
  static Theme const fallback = Theme::light();
  return fallback;
}

TextLayoutOptions textLayoutOptions(Text const& text) {
  TextLayoutOptions options{};
  options.horizontalAlignment = text.horizontalAlignment;
  options.verticalAlignment = text.verticalAlignment;
  options.wrapping = text.wrapping;
  options.maxLines = text.maxLines;
  options.firstBaselineOffset = text.firstBaselineOffset;
  return options;
}

float finiteOrZero(float value) {
  return std::isfinite(value) ? std::max(0.f, value) : 0.f;
}

Size assignedSize(LayoutConstraints const& constraints) {
  Size size{};
  if (std::isfinite(constraints.maxWidth)) {
    size.width = std::max(constraints.minWidth, constraints.maxWidth);
  } else {
    size.width = std::max(0.f, constraints.minWidth);
  }
  if (std::isfinite(constraints.maxHeight)) {
    size.height = std::max(constraints.minHeight, constraints.maxHeight);
  } else {
    size.height = std::max(0.f, constraints.minHeight);
  }
  return size;
}

bool positiveFinite(float value) {
  return std::isfinite(value) && value > 0.f;
}

float finiteSpan(float value) {
  return positiveFinite(value) ? value : 0.f;
}

bool axisStretches(std::optional<Alignment> const& alignment) {
  return alignment.has_value() && *alignment == Alignment::Stretch;
}

bool finiteWidthIsAssigned(LayoutHints const& hints) {
  return !hints.zStackHorizontalAlign.has_value() || axisStretches(hints.zStackHorizontalAlign);
}

bool fixedFiniteHeight(LayoutConstraints const& constraints) {
  return positiveFinite(constraints.maxHeight) && constraints.minHeight >= constraints.maxHeight - 1e-4f;
}

bool finiteHeightIsConstrained(LayoutConstraints const& constraints, LayoutHints const& hints) {
  return axisStretches(hints.zStackVerticalAlign) ||
         (!hints.zStackVerticalAlign.has_value() && fixedFiniteHeight(constraints));
}

LayoutConstraints fixedConstraints(Size size) {
  return LayoutConstraints{
      .maxWidth = std::max(0.f, size.width),
      .maxHeight = std::max(0.f, size.height),
      .minWidth = std::max(0.f, size.width),
      .minHeight = std::max(0.f, size.height),
  };
}

LayoutConstraints stackChildConstraints(LayoutConstraints constraints) {
  constraints.minWidth = 0.f;
  constraints.minHeight = 0.f;
  return constraints;
}

Size measureChild(Element const& child, MountContext& ctx, LayoutConstraints const& constraints,
                  LayoutHints const& hints = {}) {
  ctx.measureContext().pushConstraints(constraints, hints);
  Size measured = child.measure(ctx.measureContext(), constraints, hints, ctx.textSystem());
  ctx.measureContext().popConstraints();
  return measured;
}

std::vector<layout::StackMainAxisChild> stackChildrenForAxis(std::vector<Element> const& children,
                                                             std::vector<Size> const& sizes,
                                                             layout::StackAxis axis) {
  std::vector<layout::StackMainAxisChild> stackChildren;
  stackChildren.reserve(children.size());
  for (std::size_t i = 0; i < children.size(); ++i) {
    float const naturalMainSize = axis == layout::StackAxis::Vertical ? sizes[i].height : sizes[i].width;
    stackChildren.push_back(layout::StackMainAxisChild{
        .naturalMainSize = naturalMainSize,
        .flexBasis = children[i].flexBasis(),
        .minMainSize = children[i].minMainSize(),
        .flexGrow = children[i].flexGrow(),
        .flexShrink = children[i].flexShrink(),
    });
  }
  return stackChildren;
}

void rewindMeasuredChildren(MountContext& ctx) {
  ctx.measureContext().rewindChildKeyIndex();
}

} // namespace

MountContext::MountContext(Reactive::Scope& owner, EnvironmentStack& environment,
                           TextSystem& textSystem, MeasureContext& measureContext,
                           LayoutConstraints constraints, LayoutHints hints,
                           std::function<void()> requestRedraw)
    : owner_(owner)
    , environment_(environment)
    , textSystem_(textSystem)
    , measureContext_(measureContext)
    , constraints_(constraints)
    , hints_(hints)
    , requestRedraw_(std::move(requestRedraw)) {}

MountContext MountContext::child(LayoutConstraints constraints, LayoutHints hints) const {
  return MountContext{owner_, environment_, textSystem_, measureContext_, constraints,
                      hints, requestRedraw_};
}

void MountContext::requestRedraw() const {
  if (requestRedraw_) {
    requestRedraw_();
  }
}

namespace detail {

std::unique_ptr<scenegraph::SceneNode> mountRectangle(Rectangle const&, MountContext& ctx) {
  Size const size = assignedSize(ctx.constraints());
  return std::make_unique<scenegraph::RectNode>(
      Rect{0.f, 0.f, finiteOrZero(size.width), finiteOrZero(size.height)});
}

std::unique_ptr<scenegraph::SceneNode> mountText(Text const& text, MountContext& ctx) {
  Theme const& theme = activeTheme(ctx.environment());
  Font const font = resolveFont(text.font, theme.bodyFont, theme);
  Color const color = resolveColor(text.color.evaluate(), theme.labelColor, theme);
  TextLayoutOptions const options = textLayoutOptions(text);
  std::string const initialText = text.text.evaluate();

  Size frameSize = assignedSize(ctx.constraints());
  if (frameSize.width <= 0.f || frameSize.height <= 0.f) {
    float maxWidth = std::isfinite(ctx.constraints().maxWidth) ? ctx.constraints().maxWidth : 0.f;
    if (text.wrapping == TextWrapping::NoWrap) {
      maxWidth = 0.f;
    }
    Size measured = ctx.textSystem().measure(initialText, font, color, maxWidth, options);
    frameSize.width = frameSize.width > 0.f ? frameSize.width : measured.width;
    frameSize.height = frameSize.height > 0.f ? frameSize.height : measured.height;
  }

  Rect const box{0.f, 0.f, finiteOrZero(frameSize.width), finiteOrZero(frameSize.height)};
  auto layout = ctx.textSystem().layout(initialText, font, color, box, options);
  auto node = std::make_unique<scenegraph::TextNode>(box, std::move(layout));

  if (text.text.isReactive() || text.color.isReactive()) {
    auto* rawNode = node.get();
    Reactive::Bindable<std::string> textBinding = text.text;
    Reactive::Bindable<Color> colorBinding = text.color;
    TextSystem* textSystem = &ctx.textSystem();
    LayoutConstraints constraints = ctx.constraints();
    TextWrapping wrapping = text.wrapping;
    std::function<void()> requestRedraw = ctx.redrawCallback();
    Reactive::withOwner(ctx.owner(), [rawNode, textBinding = std::move(textBinding), textSystem,
                                      colorBinding = std::move(colorBinding), font, theme, options, constraints, wrapping,
                                      requestRedraw = std::move(requestRedraw)]() mutable {
      Reactive::Effect([rawNode, textBinding, colorBinding, textSystem, font, theme, options, constraints,
                        wrapping, requestRedraw]() mutable {
        std::string const currentText = textBinding.evaluate();
        Color const currentColor = resolveColor(colorBinding.evaluate(), theme.labelColor, theme);
        Size currentSize = rawNode->size();
        if (currentSize.width <= 0.f || currentSize.height <= 0.f) {
          float maxWidth = std::isfinite(constraints.maxWidth) ? constraints.maxWidth : 0.f;
          if (wrapping == TextWrapping::NoWrap) {
            maxWidth = 0.f;
          }
          Size const measured = textSystem->measure(currentText, font, currentColor, maxWidth, options);
          currentSize.width = currentSize.width > 0.f ? currentSize.width : measured.width;
          currentSize.height = currentSize.height > 0.f ? currentSize.height : measured.height;
          rawNode->setSize(currentSize);
        }
        Rect const currentBox{0.f, 0.f, finiteOrZero(currentSize.width), finiteOrZero(currentSize.height)};
        rawNode->setLayout(textSystem->layout(currentText, font, currentColor, currentBox, options));
        if (requestRedraw) {
          requestRedraw();
        }
      });
    });
  }

  return node;
}

std::unique_ptr<scenegraph::SceneNode> mountVStack(VStack const& stack, MountContext& ctx) {
  float const assignedWidth = finiteSpan(ctx.constraints().maxWidth);
  bool const widthAssigned = assignedWidth > 0.f && finiteWidthIsAssigned(ctx.hints());
  float const assignedHeight = finiteSpan(ctx.constraints().maxHeight);
  bool const heightConstrained = assignedHeight > 0.f &&
                                 finiteHeightIsConstrained(ctx.constraints(), ctx.hints());

  LayoutConstraints childConstraints = stackChildConstraints(ctx.constraints());
  childConstraints.maxWidth = assignedWidth > 0.f ? assignedWidth
                                                  : std::numeric_limits<float>::infinity();
  childConstraints.maxHeight = std::numeric_limits<float>::infinity();
  layout::clampLayoutMinToMax(childConstraints);

  LayoutHints childHints{};
  childHints.vStackCrossAlign = stack.alignment;
  std::vector<Size> sizes;
  sizes.reserve(stack.children.size());
  for (Element const& child : stack.children) {
    sizes.push_back(measureChild(child, ctx, childConstraints, childHints));
  }

  std::vector<layout::StackMainAxisChild> stackChildren =
      stackChildrenForAxis(stack.children, sizes, layout::StackAxis::Vertical);
  layout::StackMainAxisLayout const mainLayout =
      layout::layoutStackMainAxis(stackChildren, stack.spacing, assignedHeight, heightConstrained,
                                  stack.justifyContent);
  layout::StackLayoutResult stackLayout =
      layout::layoutStack(layout::StackAxis::Vertical, stack.alignment, sizes, mainLayout.mainSizes,
                          mainLayout.itemSpacing, mainLayout.containerMainSize,
                          mainLayout.startOffset, assignedWidth, widthAssigned);

  auto group = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, finiteOrZero(stackLayout.containerSize.width),
           finiteOrZero(stackLayout.containerSize.height)});

  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    layout::StackSlot const& slot = stackLayout.slots[i];
    MountContext childCtx = ctx.child(fixedConstraints(slot.assignedSize), childHints);
    auto node = child.mount(childCtx);
    if (node) {
      detail::setLayoutPosition(*node, slot.origin);
      group->appendChild(std::move(node));
    }
  }
  return group;
}

std::unique_ptr<scenegraph::SceneNode> mountHStack(HStack const& stack, MountContext& ctx) {
  if (stack.children.empty()) {
    return std::make_unique<scenegraph::GroupNode>();
  }

  float const assignedWidth = finiteSpan(ctx.constraints().maxWidth);
  bool const widthConstrained = assignedWidth > 0.f && finiteWidthIsAssigned(ctx.hints());
  float const assignedHeight = finiteSpan(ctx.constraints().maxHeight);
  bool const heightConstrained = assignedHeight > 0.f &&
                                 finiteHeightIsConstrained(ctx.constraints(), ctx.hints());
  bool const stretchCrossAxis = stack.alignment == Alignment::Stretch && heightConstrained;

  LayoutConstraints initialConstraints = stackChildConstraints(ctx.constraints());
  initialConstraints.maxWidth = std::numeric_limits<float>::infinity();
  initialConstraints.maxHeight = stretchCrossAxis ? assignedHeight
                                                  : std::numeric_limits<float>::infinity();
  layout::clampLayoutMinToMax(initialConstraints);

  std::vector<Size> initialSizes;
  initialSizes.reserve(stack.children.size());
  for (Element const& child : stack.children) {
    initialSizes.push_back(measureChild(child, ctx, initialConstraints, LayoutHints{}));
  }

  std::vector<layout::StackMainAxisChild> stackChildren =
      stackChildrenForAxis(stack.children, initialSizes, layout::StackAxis::Horizontal);
  layout::StackMainAxisLayout const mainLayout =
      layout::layoutStackMainAxis(stackChildren, stack.spacing, assignedWidth, widthConstrained,
                                  stack.justifyContent);

  rewindMeasuredChildren(ctx);

  LayoutHints rowHints{};
  rowHints.hStackCrossAlign = stack.alignment;
  std::vector<Size> rowSizes;
  rowSizes.reserve(stack.children.size());
  float rowInnerHeight = 0.f;
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    LayoutConstraints childMeasure = stackChildConstraints(ctx.constraints());
    childMeasure.maxWidth = i < mainLayout.mainSizes.size()
                                ? mainLayout.mainSizes[i]
                                : std::numeric_limits<float>::infinity();
    childMeasure.maxHeight = stretchCrossAxis ? assignedHeight
                                              : std::numeric_limits<float>::infinity();
    layout::clampLayoutMinToMax(childMeasure);
    Size const size = measureChild(stack.children[i], ctx, childMeasure, rowHints);
    rowSizes.push_back(size);
    rowInnerHeight = std::max(rowInnerHeight, size.height);
  }

  float const rowCrossSize = heightConstrained ? assignedHeight : rowInnerHeight;
  layout::StackLayoutResult stackLayout =
      layout::layoutStack(layout::StackAxis::Horizontal, stack.alignment, rowSizes, mainLayout.mainSizes,
                          mainLayout.itemSpacing, mainLayout.containerMainSize,
                          mainLayout.startOffset, rowCrossSize, heightConstrained);

  auto group = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, finiteOrZero(stackLayout.containerSize.width),
           finiteOrZero(stackLayout.containerSize.height)});

  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    layout::StackSlot const& slot = stackLayout.slots[i];
    MountContext childCtx = ctx.child(fixedConstraints(slot.assignedSize), rowHints);
    auto node = child.mount(childCtx);
    if (node) {
      detail::setLayoutPosition(*node, slot.origin);
      group->appendChild(std::move(node));
    }
  }
  return group;
}

std::unique_ptr<scenegraph::SceneNode> mountZStack(ZStack const& stack, MountContext& ctx) {
  float const assignedWidth = finiteSpan(ctx.constraints().maxWidth);
  float const assignedHeight = finiteSpan(ctx.constraints().maxHeight);
  float width = assignedWidth;
  float height = assignedHeight;

  LayoutConstraints childMeasure = stackChildConstraints(ctx.constraints());
  childMeasure.maxWidth = assignedWidth > 0.f ? assignedWidth : std::numeric_limits<float>::infinity();
  childMeasure.maxHeight = assignedHeight > 0.f ? assignedHeight : std::numeric_limits<float>::infinity();
  layout::clampLayoutMinToMax(childMeasure);

  LayoutHints childHints{};
  childHints.zStackHorizontalAlign = stack.horizontalAlignment;
  childHints.zStackVerticalAlign = stack.verticalAlignment;
  std::vector<Size> sizes;
  sizes.reserve(stack.children.size());
  for (Element const& child : stack.children) {
    Size const size = measureChild(child, ctx, childMeasure, childHints);
    sizes.push_back(size);
    width = std::max(width, size.width);
    height = std::max(height, size.height);
  }
  if (assignedWidth > 0.f) {
    width = std::min(width, assignedWidth);
  }
  if (assignedHeight > 0.f) {
    height = std::min(height, assignedHeight);
  }

  auto group = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, finiteOrZero(width), finiteOrZero(height)});

  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    Size childFrame = i < sizes.size() ? sizes[i] : Size{};
    if (stack.horizontalAlignment == Alignment::Stretch) {
      childFrame.width = width;
    }
    if (stack.verticalAlignment == Alignment::Stretch) {
      childFrame.height = height;
    }
    MountContext childCtx = ctx.child(fixedConstraints(childFrame), childHints);
    auto node = child.mount(childCtx);
    if (node) {
      detail::setLayoutPosition(*node, Point{
          layout::hAlignOffset(childFrame.width, width, stack.horizontalAlignment),
          layout::vAlignOffset(childFrame.height, height, stack.verticalAlignment),
      });
      group->appendChild(std::move(node));
    }
  }
  return group;
}

std::unique_ptr<scenegraph::SceneNode> mountSpacer(Spacer const&, MountContext& ctx) {
  (void)ctx;
  Size const size{};
  return std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, finiteOrZero(size.width), finiteOrZero(size.height)});
}

} // namespace detail
} // namespace flux
