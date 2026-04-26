#include <Flux/UI/MountContext.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/TextNode.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <Flux/Reactive/Effect.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

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

LayoutConstraints fixedConstraints(Size size) {
  return LayoutConstraints{
      .maxWidth = std::max(0.f, size.width),
      .maxHeight = std::max(0.f, size.height),
      .minWidth = std::max(0.f, size.width),
      .minHeight = std::max(0.f, size.height),
  };
}

Size measureChild(Element const& child, MountContext& ctx, LayoutConstraints const& constraints,
                  LayoutHints const& hints = {}) {
  ctx.measureContext().pushConstraints(constraints, hints);
  Size measured = child.measure(ctx.measureContext(), constraints, hints, ctx.textSystem());
  ctx.measureContext().popConstraints();
  return measured;
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
  Size const frameSize = assignedSize(ctx.constraints());
  auto group = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, finiteOrZero(frameSize.width), finiteOrZero(frameSize.height)});

  float y = 0.f;
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LayoutConstraints childConstraints = ctx.constraints();
    childConstraints.minWidth = 0.f;
    childConstraints.minHeight = 0.f;
    childConstraints.maxHeight = std::numeric_limits<float>::infinity();
    LayoutHints childHints{};
    childHints.vStackCrossAlign = stack.alignment;
    Size const measured = measureChild(child, ctx, childConstraints, childHints);
    MountContext childCtx = ctx.child(fixedConstraints(measured), childHints);
    auto node = child.mount(childCtx);
    if (node) {
      node->setPosition(Point{0.f, y});
      group->appendChild(std::move(node));
    }
    y += measured.height;
    if (i + 1 < stack.children.size()) {
      y += stack.spacing;
    }
  }
  group->setSize(Size{std::max(frameSize.width, group->size().width), std::max(frameSize.height, y)});
  return group;
}

std::unique_ptr<scenegraph::SceneNode> mountHStack(HStack const& stack, MountContext& ctx) {
  Size const frameSize = assignedSize(ctx.constraints());
  auto group = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, finiteOrZero(frameSize.width), finiteOrZero(frameSize.height)});

  float x = 0.f;
  float maxHeight = frameSize.height;
  for (std::size_t i = 0; i < stack.children.size(); ++i) {
    Element const& child = stack.children[i];
    LayoutConstraints childConstraints = ctx.constraints();
    childConstraints.minWidth = 0.f;
    childConstraints.minHeight = 0.f;
    childConstraints.maxWidth = std::numeric_limits<float>::infinity();
    LayoutHints childHints{};
    childHints.hStackCrossAlign = stack.alignment;
    Size const measured = measureChild(child, ctx, childConstraints, childHints);
    MountContext childCtx = ctx.child(fixedConstraints(measured), childHints);
    auto node = child.mount(childCtx);
    if (node) {
      node->setPosition(Point{x, 0.f});
      group->appendChild(std::move(node));
    }
    x += measured.width;
    if (i + 1 < stack.children.size()) {
      x += stack.spacing;
    }
    maxHeight = std::max(maxHeight, measured.height);
  }
  group->setSize(Size{std::max(frameSize.width, x), maxHeight});
  return group;
}

std::unique_ptr<scenegraph::SceneNode> mountZStack(ZStack const& stack, MountContext& ctx) {
  Size const frameSize = assignedSize(ctx.constraints());
  auto group = std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, finiteOrZero(frameSize.width), finiteOrZero(frameSize.height)});

  LayoutHints childHints{};
  childHints.zStackHorizontalAlign = stack.horizontalAlignment;
  childHints.zStackVerticalAlign = stack.verticalAlignment;
  for (Element const& child : stack.children) {
    MountContext childCtx = ctx.child(ctx.constraints(), childHints);
    auto node = child.mount(childCtx);
    if (node) {
      group->appendChild(std::move(node));
    }
  }
  return group;
}

std::unique_ptr<scenegraph::SceneNode> mountSpacer(Spacer const&, MountContext& ctx) {
  Size const size = assignedSize(ctx.constraints());
  return std::make_unique<scenegraph::GroupNode>(
      Rect{0.f, 0.f, finiteOrZero(size.width), finiteOrZero(size.height)});
}

} // namespace detail
} // namespace flux
