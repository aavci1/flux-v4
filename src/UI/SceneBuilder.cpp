#include <Flux/UI/SceneBuilder.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/CustomTransformSceneNode.hpp>
#include <Flux/Scene/ImageSceneNode.hpp>
#include <Flux/Scene/InteractionData.hpp>
#include <Flux/Scene/LineSceneNode.hpp>
#include <Flux/Scene/ModifierSceneNode.hpp>
#include <Flux/Scene/PathSceneNode.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/RenderSceneNode.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/Scene/TextSceneNode.hpp>
#include <Flux/UI/Detail/LeafBounds.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Grid.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Image.hpp>
#include <Flux/UI/Views/Line.hpp>
#include <Flux/UI/Views/OffsetView.hpp>
#include <Flux/UI/Views/PathShape.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextSupport.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include "UI/Layout/LayoutHelpers.hpp"

namespace flux {

namespace {

using layout::assignedSpan;
using layout::clampLayoutMinToMax;
using layout::flexGrowAlongMainAxis;
using layout::flexShrinkAlongMainAxis;
using layout::hAlignOffset;
using layout::kFlexEpsilon;
using layout::stackMainAxisSpan;
using layout::vAlignOffset;
using layout::warnFlexGrowIfParentMainAxisUnconstrained;

constexpr float kApproxEpsilon = 1e-4f;

bool nearlyEqual(float lhs, float rhs, float eps = kApproxEpsilon) {
  return std::fabs(lhs - rhs) <= eps;
}

bool nearlyEqual(Size lhs, Size rhs) {
  return nearlyEqual(lhs.width, rhs.width) && nearlyEqual(lhs.height, rhs.height);
}

Rect unionRect(Rect lhs, Rect rhs) {
  if ((lhs.width <= 0.f && lhs.height <= 0.f)) {
    return rhs;
  }
  if ((rhs.width <= 0.f && rhs.height <= 0.f)) {
    return lhs;
  }
  float const x0 = std::min(lhs.x, rhs.x);
  float const y0 = std::min(lhs.y, rhs.y);
  float const x1 = std::max(lhs.x + lhs.width, rhs.x + rhs.width);
  float const y1 = std::max(lhs.y + lhs.height, rhs.y + rhs.height);
  return Rect{x0, y0, x1 - x0, y1 - y0};
}

Rect offsetRect(Rect rect, Point delta) {
  rect.x += delta.x;
  rect.y += delta.y;
  return rect;
}

Size rectSize(Rect rect) {
  return Size{std::max(0.f, rect.width), std::max(0.f, rect.height)};
}

Rect sizeRect(Size size) {
  return Rect{0.f, 0.f, std::max(0.f, size.width), std::max(0.f, size.height)};
}

bool isPlainGroup(SceneNode const& node) {
  return node.kind() == SceneNodeKind::Group && typeid(node) == typeid(SceneNode);
}

template<typename T>
std::unique_ptr<T> releaseAs(std::unique_ptr<SceneNode> node) {
  if (!node) {
    return nullptr;
  }
  if (T* typed = dynamic_cast<T*>(node.get())) {
    node.release();
    return std::unique_ptr<T>(typed);
  }
  return nullptr;
}

std::unique_ptr<SceneNode> releasePlainGroup(std::unique_ptr<SceneNode> node) {
  if (node && isPlainGroup(*node)) {
    return node;
  }
  return nullptr;
}

void setGroupBounds(SceneNode& node, Size minSize = {}) {
  Rect bounds = sizeRect(minSize);
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    bounds = unionRect(bounds, offsetRect(child->bounds, child->position));
  }
  node.bounds = bounds;
  node.markBoundsDirty();
}

LayoutConstraints insetConstraints(LayoutConstraints constraints, EdgeInsets const& padding) {
  float const dx = std::max(0.f, padding.left) + std::max(0.f, padding.right);
  float const dy = std::max(0.f, padding.top) + std::max(0.f, padding.bottom);
  if (std::isfinite(constraints.maxWidth)) {
    constraints.maxWidth = std::max(0.f, constraints.maxWidth - dx);
  }
  if (std::isfinite(constraints.maxHeight)) {
    constraints.maxHeight = std::max(0.f, constraints.maxHeight - dy);
  }
  constraints.minWidth = std::max(0.f, constraints.minWidth - dx);
  constraints.minHeight = std::max(0.f, constraints.minHeight - dy);
  clampLayoutMinToMax(constraints);
  return constraints;
}

Point modifierOffset(ElementModifiers const* mods) {
  if (!mods) {
    return {};
  }
  return Point{mods->positionX + mods->translation.x, mods->positionY + mods->translation.y};
}

bool needsModifierWrapper(ElementModifiers const* mods, bool leafOwnsPaint) {
  if (!mods) {
    return false;
  }
  if (mods->clip || mods->opacity < 1.f - 1e-6f) {
    return true;
  }
  if (leafOwnsPaint) {
    return false;
  }
  return !mods->fill.isNone() || !mods->stroke.isNone() || !mods->shadow.isNone() || !mods->cornerRadius.isZero();
}

bool hasOverlay(ElementModifiers const* mods) {
  return mods && mods->overlay != nullptr;
}

bool hasPadding(ElementModifiers const* mods) {
  return mods && !mods->padding.isZero();
}

bool needsLayoutWrapper(ElementModifiers const* mods, Size outerSize, Size contentSize) {
  if (!mods) {
    return false;
  }
  if (hasPadding(mods) || hasOverlay(mods)) {
    return true;
  }
  return !nearlyEqual(outerSize, contentSize);
}

std::unique_ptr<InteractionData> makeInteractionData(ElementModifiers const* mods, ComponentKey const& key) {
  if (!mods) {
    return nullptr;
  }
  auto data = std::make_unique<InteractionData>();
  data->stableTargetKey = key;
  data->cursor = mods->cursor;
  data->focusable = mods->focusable || static_cast<bool>(mods->onKeyDown) ||
                    static_cast<bool>(mods->onKeyUp) || static_cast<bool>(mods->onTextInput);
  data->onPointerDown = mods->onPointerDown;
  data->onPointerUp = mods->onPointerUp;
  data->onPointerMove = mods->onPointerMove;
  data->onScroll = mods->onScroll;
  data->onKeyDown = mods->onKeyDown;
  data->onKeyUp = mods->onKeyUp;
  data->onTextInput = mods->onTextInput;
  data->onTap = mods->onTap;
  if (data->isEmpty()) {
    return nullptr;
  }
  return data;
}

LocalId childLocalId(Element const& child, std::size_t index) {
  if (child.explicitKey()) {
    return LocalId::fromString(*child.explicitKey());
  }
  return LocalId::fromIndex(index);
}

Rect assignedFrameForLeaf(Size size, ElementModifiers const* mods, LayoutHints const& hints) {
  Rect explicitBox{};
  if (mods) {
    explicitBox.width = std::max(0.f, mods->sizeWidth);
    explicitBox.height = std::max(0.f, mods->sizeHeight);
  }
  Rect const childFrame{0.f, 0.f, size.width, size.height};
  LayoutConstraints constraints{};
  constraints.maxWidth = size.width;
  constraints.maxHeight = size.height;
  constraints.minWidth = size.width;
  constraints.minHeight = size.height;
  return detail::resolveLeafLayoutBounds(explicitBox, childFrame, constraints, hints);
}

bool updateIfChanged(Size& field, Size value) {
  if (field == value) {
    return false;
  }
  field = value;
  return true;
}

bool updateIfChanged(Point& field, Point value) {
  if (field == value) {
    return false;
  }
  field = value;
  return true;
}

template<typename T>
bool updateIfChanged(T& field, T const& value) {
  if (field == value) {
    return false;
  }
  field = value;
  return true;
}

bool updateIfChanged(Font& field, Font const& value) {
  if (field.family == value.family && nearlyEqual(field.size, value.size) &&
      nearlyEqual(field.weight, value.weight) && field.italic == value.italic) {
    return false;
  }
  field = value;
  return true;
}

bool updateIfChanged(Path& field, Path const& value) {
  if (field.contentHash() == value.contentHash()) {
    return false;
  }
  field = value;
  return true;
}

LayoutConstraints scrollChildConstraints(ScrollAxis axis, LayoutConstraints constraints, Size viewport) {
  switch (axis) {
  case ScrollAxis::Vertical:
    constraints.maxWidth = viewport.width > 0.f ? viewport.width : std::numeric_limits<float>::infinity();
    constraints.maxHeight = std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Horizontal:
    constraints.maxWidth = std::numeric_limits<float>::infinity();
    constraints.maxHeight = viewport.height > 0.f ? viewport.height : std::numeric_limits<float>::infinity();
    break;
  case ScrollAxis::Both:
    constraints.maxWidth = std::numeric_limits<float>::infinity();
    constraints.maxHeight = std::numeric_limits<float>::infinity();
    break;
  }
  clampLayoutMinToMax(constraints);
  return constraints;
}

Size scrollContentSize(ScrollAxis axis, std::vector<Size> const& sizes) {
  float totalW = 0.f;
  float totalH = 0.f;
  switch (axis) {
  case ScrollAxis::Horizontal:
    for (Size const size : sizes) {
      totalW += size.width;
      totalH = std::max(totalH, size.height);
    }
    break;
  case ScrollAxis::Vertical:
    for (Size const size : sizes) {
      totalW = std::max(totalW, size.width);
      totalH += size.height;
    }
    break;
  case ScrollAxis::Both:
    for (Size const size : sizes) {
      totalW = std::max(totalW, size.width);
      totalH = std::max(totalH, size.height);
    }
    break;
  }
  return Size{totalW, totalH};
}

bool sizeApproximatelyEqual(Size lhs, Size rhs) {
  return nearlyEqual(lhs.width, rhs.width, 0.5f) && nearlyEqual(lhs.height, rhs.height, 0.5f);
}

} // namespace

SceneBuilder::SceneBuilder(TextSystem& textSystem, EnvironmentStack& environment,
                           SceneGeometryIndex* geometryIndex)
    : textSystem_(textSystem)
    , environment_(environment)
    , geometryIndex_(geometryIndex) {}

SceneBuilder::FrameState const& SceneBuilder::frame() const {
  return frames_.back();
}

void SceneBuilder::pushFrame(LayoutConstraints const& constraints, LayoutHints const& hints, Point origin,
                             ComponentKey key) {
  frames_.push_back(FrameState{
      .constraints = constraints,
      .hints = hints,
      .origin = origin,
      .key = std::move(key),
  });
}

void SceneBuilder::popFrame() {
  frames_.pop_back();
}

Size SceneBuilder::measureElement(Element const& el, LayoutConstraints const& constraints,
                                  LayoutHints const& hints) const {
  LayoutEngine layoutEngine{};
  layoutEngine.resetForBuild();
  LayoutTree layoutTree{};
  LayoutContext layoutContext{textSystem_, layoutEngine, layoutTree};
  layoutContext.pushConstraints(constraints, hints);
  layoutContext.setCurrentElement(&el);
  return el.measure(layoutContext, constraints, hints, textSystem_);
}

std::unique_ptr<SceneNode> SceneBuilder::build(Element const& el, NodeId id,
                                               LayoutConstraints const& constraints,
                                               std::unique_ptr<SceneNode> existing,
                                               ComponentKey rootKey) {
  if (geometryIndex_) {
    geometryIndex_->beginBuild();
  }
  pushFrame(constraints, LayoutHints{}, Point{}, std::move(rootKey));
  std::unique_ptr<SceneNode> node = buildOrReuse(el, id, std::move(existing));
  popFrame();
  if (geometryIndex_) {
    geometryIndex_->finishBuild();
  }
  return node;
}

std::unique_ptr<SceneNode> SceneBuilder::buildOrReuse(Element const& el, NodeId id,
                                                      std::unique_ptr<SceneNode> existing) {
  if (frames_.empty()) {
    return build(el, id, LayoutConstraints{}, std::move(existing));
  }

  bool pushedEnv = false;
  if (EnvironmentLayer const* envLayer = el.environmentLayer()) {
    environment_.push(*envLayer);
    pushedEnv = true;
  }

  struct EnvPop {
    EnvironmentStack* env = nullptr;
    ~EnvPop() {
      if (env) {
        env->pop();
      }
    }
  } envPop{pushedEnv ? &environment_ : nullptr};

  if (el.typeTag() == ElementType::Unknown && el.isComposite()) {
    detail::CompositeBodyResolution resolution = el.resolveCompositeBody(frame().key, frame().constraints);
    if (resolution.body) {
      StateStore* const store = StateStore::current();
      if (store) {
        store->pushCompositePathStable(resolution.descendantsStable);
      }
      struct CompositePathStablePop {
        StateStore* store = nullptr;
        ~CompositePathStablePop() {
          if (store) {
            store->popCompositePathStable();
          }
        }
      } stablePop{store};
      return buildResolved(el, *resolution.body, id, std::move(existing));
    }
  }

  return buildResolved(el, el, id, std::move(existing));
}

void SceneBuilder::reconcileChildren(SceneNode& parent, std::span<Element const> newChildren,
                                     std::vector<std::unique_ptr<SceneNode>>& existingChildren) {
  std::unordered_map<NodeId, std::unique_ptr<SceneNode>, ::flux::NodeIdHash> reusable{};
  reusable.reserve(existingChildren.size());
  for (std::unique_ptr<SceneNode>& child : existingChildren) {
    if (child) {
      reusable.emplace(child->id(), std::move(child));
    }
  }

  std::vector<std::unique_ptr<SceneNode>> next{};
  next.reserve(newChildren.size());
  for (std::size_t i = 0; i < newChildren.size(); ++i) {
    Element const& child = newChildren[i];
    NodeId const childId = SceneTree::childId(parent.id(), childLocalId(child, i));
    std::unique_ptr<SceneNode> reuse{};
    if (auto it = reusable.find(childId); it != reusable.end()) {
      reuse = std::move(it->second);
      reusable.erase(it);
    }
    next.push_back(buildOrReuse(child, childId, std::move(reuse)));
  }
  parent.replaceChildren(std::move(next));
}

std::unique_ptr<SceneNode> SceneBuilder::buildResolved(Element const& el, Element const& sceneEl, NodeId id,
                                                       std::unique_ptr<SceneNode> existing) {
  FrameState const current = frame();
  ElementModifiers const* mods = el.modifiers();
  Point const subtreeOffset = modifierOffset(mods);
  Size const outerSize = measureElement(el, current.constraints, current.hints);
  EdgeInsets const padding = mods ? mods->padding : EdgeInsets{};
  LayoutConstraints innerConstraints = insetConstraints(current.constraints, padding);
  Size const paddedContentSize{
      std::max(0.f, outerSize.width - std::max(0.f, padding.left) - std::max(0.f, padding.right)),
      std::max(0.f, outerSize.height - std::max(0.f, padding.top) - std::max(0.f, padding.bottom)),
  };
  Point const contentOrigin{
      current.origin.x + subtreeOffset.x + std::max(0.f, padding.left),
      current.origin.y + subtreeOffset.y + std::max(0.f, padding.top),
  };

  auto recordGeometry = [&](Size size) {
    if (geometryIndex_) {
      geometryIndex_->record(
          current.key, Rect{current.origin.x + subtreeOffset.x, current.origin.y + subtreeOffset.y,
                            std::max(0.f, size.width), std::max(0.f, size.height)});
    }
  };

  std::unique_ptr<SceneNode> core{};
  switch (sceneEl.typeTag()) {
  case ElementType::Rectangle: {
    std::unique_ptr<RectSceneNode> rectNode = releaseAs<RectSceneNode>(std::move(existing));
    if (!rectNode) {
      rectNode = std::make_unique<RectSceneNode>(id);
    }
    Size const resolvedRectSize = rectSize(assignedFrameForLeaf(paddedContentSize, mods, current.hints));
    bool dirty = false;
    dirty |= updateIfChanged(rectNode->size, resolvedRectSize);
    dirty |= updateIfChanged(rectNode->cornerRadius, mods ? mods->cornerRadius : CornerRadius{});
    dirty |= updateIfChanged(rectNode->fill, mods ? mods->fill : FillStyle::none());
    dirty |= updateIfChanged(rectNode->stroke, mods ? mods->stroke : StrokeStyle::none());
    dirty |= updateIfChanged(rectNode->shadow, mods ? mods->shadow : ShadowStyle::none());
    if (dirty) {
      rectNode->markPaintDirty();
      rectNode->markBoundsDirty();
    }
    rectNode->position = {};
    rectNode->recomputeBounds();
    core = std::move(rectNode);
    break;
  }
  case ElementType::Text: {
    Text const& text = sceneEl.as<Text>();
    std::unique_ptr<TextSceneNode> textNode = releaseAs<TextSceneNode>(std::move(existing));
    if (!textNode) {
      textNode = std::make_unique<TextSceneNode>(id);
    }
    auto const [resolvedFont, resolvedColor] = text_detail::resolveBodyTextStyle(text.font, text.color);
    Rect const frameRect = assignedFrameForLeaf(paddedContentSize, mods, current.hints);
    TextLayoutOptions const options = text_detail::makeTextLayoutOptions(text);
    std::string const displayText =
        text.selectable ? text.text
                        : text_detail::ellipsizedPlainText(text.text, resolvedFont, resolvedColor, frameRect,
                                                            options, textSystem_);
    bool dirty = false;
    dirty |= updateIfChanged(textNode->text, displayText);
    dirty |= updateIfChanged(textNode->font, resolvedFont);
    dirty |= updateIfChanged(textNode->color, resolvedColor);
    dirty |= updateIfChanged(textNode->horizontalAlignment, text.horizontalAlignment);
    dirty |= updateIfChanged(textNode->verticalAlignment, text.verticalAlignment);
    dirty |= updateIfChanged(textNode->wrapping, text.wrapping);
    dirty |= updateIfChanged(textNode->maxLines, text.maxLines);
    dirty |= updateIfChanged(textNode->firstBaselineOffset, text.firstBaselineOffset);
    dirty |= updateIfChanged(textNode->widthConstraint,
                             text.wrapping == TextWrapping::NoWrap ? 0.f : frameRect.width);
    dirty |= updateIfChanged(textNode->origin, Point{0.f, 0.f});
    dirty |= updateIfChanged(textNode->allocation, Rect{0.f, 0.f, frameRect.width, frameRect.height});
    if (textNode->textSystem != &textSystem_) {
      textNode->textSystem = &textSystem_;
      dirty = true;
    }
    if (dirty) {
      textNode->layout.reset();
      textNode->markPaintDirty();
      textNode->markBoundsDirty();
    }
    textNode->position = {};
    textNode->recomputeBounds();
    core = std::move(textNode);
    break;
  }
  case ElementType::Render: {
    Render const& renderView = sceneEl.as<Render>();
    std::unique_ptr<RenderSceneNode> renderNode = releaseAs<RenderSceneNode>(std::move(existing));
    if (!renderNode) {
      renderNode = std::make_unique<RenderSceneNode>(id);
    }
    Rect const frameRect{0.f, 0.f, paddedContentSize.width, paddedContentSize.height};
    bool dirty = false;
    dirty |= updateIfChanged(renderNode->frame, frameRect);
    if (renderNode->pure != renderView.pure) {
      renderNode->pure = renderView.pure;
      dirty = true;
    }
    if (!renderNode->pure || !renderNode->draw || renderNode->draw.target_type() != renderView.draw.target_type()) {
      renderNode->draw = renderView.draw;
      dirty = true;
    }
    if (dirty) {
      renderNode->markPaintDirty();
      renderNode->markBoundsDirty();
    }
    renderNode->position = {};
    renderNode->recomputeBounds();
    core = std::move(renderNode);
    break;
  }
  case ElementType::Image: {
    views::Image const& image = sceneEl.as<views::Image>();
    std::unique_ptr<ImageSceneNode> imageNode = releaseAs<ImageSceneNode>(std::move(existing));
    if (!imageNode) {
      imageNode = std::make_unique<ImageSceneNode>(id);
    }
    Rect const frameRect = assignedFrameForLeaf(paddedContentSize, mods, current.hints);
    bool dirty = false;
    dirty |= updateIfChanged(imageNode->image, image.source);
    dirty |= updateIfChanged(imageNode->size, Size{frameRect.width, frameRect.height});
    dirty |= updateIfChanged(imageNode->fillMode, image.fillMode);
    dirty |= updateIfChanged(imageNode->cornerRadius, mods ? mods->cornerRadius : CornerRadius{});
    dirty |= updateIfChanged(imageNode->opacity, mods ? mods->opacity : 1.f);
    if (dirty) {
      imageNode->markPaintDirty();
      imageNode->markBoundsDirty();
    }
    imageNode->position = {};
    imageNode->recomputeBounds();
    core = std::move(imageNode);
    break;
  }
  case ElementType::Path: {
    PathShape const& path = sceneEl.as<PathShape>();
    std::unique_ptr<PathSceneNode> pathNode = releaseAs<PathSceneNode>(std::move(existing));
    if (!pathNode) {
      pathNode = std::make_unique<PathSceneNode>(id);
    }
    bool dirty = false;
    dirty |= updateIfChanged(pathNode->path, path.path);
    dirty |= updateIfChanged(pathNode->fill, mods ? mods->fill : FillStyle::none());
    dirty |= updateIfChanged(pathNode->stroke, mods ? mods->stroke : StrokeStyle::none());
    dirty |= updateIfChanged(pathNode->shadow, mods ? mods->shadow : ShadowStyle::none());
    if (dirty) {
      pathNode->markPaintDirty();
      pathNode->markBoundsDirty();
    }
    pathNode->position = {};
    pathNode->recomputeBounds();
    core = std::move(pathNode);
    break;
  }
  case ElementType::Line: {
    Line const& line = sceneEl.as<Line>();
    std::unique_ptr<LineSceneNode> lineNode = releaseAs<LineSceneNode>(std::move(existing));
    if (!lineNode) {
      lineNode = std::make_unique<LineSceneNode>(id);
    }
    bool dirty = false;
    dirty |= updateIfChanged(lineNode->from, line.from);
    dirty |= updateIfChanged(lineNode->to, line.to);
    dirty |= updateIfChanged(lineNode->stroke, line.stroke);
    if (dirty) {
      lineNode->markPaintDirty();
      lineNode->markBoundsDirty();
    }
    lineNode->position = {};
    lineNode->recomputeBounds();
    core = std::move(lineNode);
    break;
  }
  case ElementType::VStack: {
    VStack const& stack = sceneEl.as<VStack>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(existing));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    std::vector<std::unique_ptr<SceneNode>> existingChildren = group->releaseChildren();
    std::unordered_map<NodeId, std::unique_ptr<SceneNode>, ::flux::NodeIdHash> reusable{};
    reusable.reserve(existingChildren.size());
    for (std::unique_ptr<SceneNode>& child : existingChildren) {
      if (child) {
        reusable.emplace(child->id(), std::move(child));
      }
    }

    float const assignedW = stackMainAxisSpan(0.f, innerConstraints.maxWidth);
    float const assignedH = stackMainAxisSpan(0.f, innerConstraints.maxHeight);
    float innerW = std::max(0.f, assignedW);
    LayoutConstraints childConstraints = innerConstraints;
    childConstraints.maxHeight = std::numeric_limits<float>::infinity();
    childConstraints.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
    LayoutHints childHints{};
    childHints.vStackCrossAlign = stack.alignment;

    std::vector<Size> sizes{};
    sizes.reserve(stack.children.size());
    float maxChildW = 0.f;
    for (Element const& child : stack.children) {
      Size const size = measureElement(child, childConstraints, childHints);
      sizes.push_back(size);
      maxChildW = std::max(maxChildW, size.width);
    }
    if (innerW <= 0.f) {
      innerW = maxChildW;
    }

    std::vector<float> allocH(stack.children.size(), 0.f);
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      allocH[i] = std::max(sizes[i].height, stack.children[i].minMainSize());
    }
    bool const heightConstrained = std::isfinite(assignedH) && assignedH > 0.f;
    if (heightConstrained && !allocH.empty()) {
      float const gaps = stack.children.size() > 1 ? static_cast<float>(stack.children.size() - 1) * stack.spacing : 0.f;
      float const targetSum = std::max(0.f, assignedH - gaps);
      float sumNat = 0.f;
      for (float h : allocH) {
        sumNat += h;
      }
      float const extra = targetSum - sumNat;
      if (extra > kFlexEpsilon) {
        flexGrowAlongMainAxis(allocH, stack.children, extra);
      } else if (extra < -kFlexEpsilon) {
        flexShrinkAlongMainAxis(allocH, stack.children, targetSum);
      }
    } else {
      warnFlexGrowIfParentMainAxisUnconstrained(stack.children, heightConstrained);
    }

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(stack.children.size());
    float usedH = stack.children.size() > 1 ? static_cast<float>(stack.children.size() - 1) * stack.spacing : 0.f;
    for (float h : allocH) {
      usedH += h;
    }
    float y = heightConstrained ? std::max(0.f, (assignedH - usedH) * 0.5f) : 0.f;
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      Element const& child = stack.children[i];
      LocalId const local = childLocalId(child, i);
      NodeId const childId = SceneTree::childId(id, local);
      std::unique_ptr<SceneNode> reuse{};
      if (auto it = reusable.find(childId); it != reusable.end()) {
        reuse = std::move(it->second);
        reusable.erase(it);
      }
      LayoutConstraints childBuild = innerConstraints;
      childBuild.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
      childBuild.maxHeight = allocH[i];
      childBuild.minHeight = child.minMainSize();
      clampLayoutMinToMax(childBuild);
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      pushFrame(childBuild, childHints, Point{contentOrigin.x, contentOrigin.y + y}, std::move(childKey));
      std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
      popFrame();
      childNode->position.x += hAlignOffset(childNode->bounds.width, innerW > 0.f ? innerW : childNode->bounds.width,
                                            stack.alignment);
      childNode->position.y += y;
      nextChildren.push_back(std::move(childNode));
      y += allocH[i] + stack.spacing;
    }
    group->replaceChildren(std::move(nextChildren));
    setGroupBounds(*group, Size{std::max(maxChildW, assignedW), std::max(usedH, 0.f)});
    core = std::move(group);
    break;
  }
  case ElementType::HStack: {
    HStack const& stack = sceneEl.as<HStack>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(existing));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    std::vector<std::unique_ptr<SceneNode>> existingChildren = group->releaseChildren();
    std::unordered_map<NodeId, std::unique_ptr<SceneNode>, ::flux::NodeIdHash> reusable{};
    reusable.reserve(existingChildren.size());
    for (std::unique_ptr<SceneNode>& child : existingChildren) {
      if (child) {
        reusable.emplace(child->id(), std::move(child));
      }
    }

    float const assignedW = stackMainAxisSpan(0.f, innerConstraints.maxWidth);
    float const assignedH = stackMainAxisSpan(0.f, innerConstraints.maxHeight);
    bool const heightConstrained = std::isfinite(assignedH) && assignedH > 0.f;
    LayoutConstraints childConstraints = innerConstraints;
    childConstraints.maxWidth = std::numeric_limits<float>::infinity();
    childConstraints.maxHeight = heightConstrained ? assignedH : std::numeric_limits<float>::infinity();
    if (stack.children.size() == 1 && std::isfinite(innerConstraints.maxWidth) && innerConstraints.maxWidth > 0.f) {
      childConstraints.maxWidth = innerConstraints.maxWidth;
    }

    std::vector<Size> sizes{};
    sizes.reserve(stack.children.size());
    for (Element const& child : stack.children) {
      sizes.push_back(measureElement(child, childConstraints, LayoutHints{}));
    }

    std::vector<float> allocW(stack.children.size(), 0.f);
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      allocW[i] = std::max(sizes[i].width, stack.children[i].minMainSize());
    }
    bool const widthConstrained = std::isfinite(assignedW) && assignedW > 0.f;
    if (widthConstrained && !allocW.empty()) {
      float const gaps = stack.children.size() > 1 ? static_cast<float>(stack.children.size() - 1) * stack.spacing : 0.f;
      float const targetSum = std::max(0.f, assignedW - gaps);
      float sumNat = 0.f;
      for (float w : allocW) {
        sumNat += w;
      }
      float const extra = targetSum - sumNat;
      if (extra > kFlexEpsilon) {
        flexGrowAlongMainAxis(allocW, stack.children, extra);
      } else if (extra < -kFlexEpsilon) {
        flexShrinkAlongMainAxis(allocW, stack.children, targetSum);
      }
    } else {
      warnFlexGrowIfParentMainAxisUnconstrained(stack.children, widthConstrained);
    }

    float rowInnerH = 0.f;
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      LayoutConstraints childMeasure = innerConstraints;
      childMeasure.maxWidth = allocW[i];
      childMeasure.maxHeight = heightConstrained ? assignedH : std::numeric_limits<float>::infinity();
      LayoutHints rowHints{};
      rowHints.hStackCrossAlign = stack.alignment;
      Size const measured = measureElement(stack.children[i], childMeasure, rowHints);
      rowInnerH = std::max(rowInnerH, measured.height);
    }
    if (stack.alignment == Alignment::Stretch && heightConstrained) {
      rowInnerH = std::max(rowInnerH, assignedH);
    }

    float usedW = stack.children.size() > 1 ? static_cast<float>(stack.children.size() - 1) * stack.spacing : 0.f;
    for (float w : allocW) {
      usedW += w;
    }
    float x = widthConstrained ? std::max(0.f, (assignedW - usedW) * 0.5f) : 0.f;

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(stack.children.size());
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      Element const& child = stack.children[i];
      LocalId const local = childLocalId(child, i);
      NodeId const childId = SceneTree::childId(id, local);
      std::unique_ptr<SceneNode> reuse{};
      if (auto it = reusable.find(childId); it != reusable.end()) {
        reuse = std::move(it->second);
        reusable.erase(it);
      }
      LayoutConstraints childBuild = innerConstraints;
      childBuild.maxWidth = allocW[i];
      childBuild.maxHeight = heightConstrained ? assignedH : rowInnerH;
      childBuild.minWidth = child.minMainSize();
      clampLayoutMinToMax(childBuild);
      LayoutHints rowHints{};
      rowHints.hStackCrossAlign = stack.alignment;
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      pushFrame(childBuild, rowHints, Point{contentOrigin.x + x, contentOrigin.y}, std::move(childKey));
      std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
      popFrame();
      childNode->position.x += x;
      childNode->position.y += vAlignOffset(childNode->bounds.height,
                                            stack.alignment == Alignment::Stretch && rowInnerH > 0.f ? rowInnerH
                                                                                                      : std::max(rowInnerH, childNode->bounds.height),
                                            stack.alignment);
      nextChildren.push_back(std::move(childNode));
      x += allocW[i] + stack.spacing;
    }
    group->replaceChildren(std::move(nextChildren));
    setGroupBounds(*group, Size{std::max(usedW, 0.f), std::max(rowInnerH, 0.f)});
    core = std::move(group);
    break;
  }
  case ElementType::ZStack: {
    ZStack const& stack = sceneEl.as<ZStack>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(existing));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    std::vector<std::unique_ptr<SceneNode>> existingChildren = group->releaseChildren();
    std::unordered_map<NodeId, std::unique_ptr<SceneNode>, ::flux::NodeIdHash> reusable{};
    reusable.reserve(existingChildren.size());
    for (std::unique_ptr<SceneNode>& child : existingChildren) {
      if (child) {
        reusable.emplace(child->id(), std::move(child));
      }
    }

    float innerW = std::max(0.f, assignedSpan(0.f, innerConstraints.maxWidth));
    float innerH = std::max(0.f, assignedSpan(0.f, innerConstraints.maxHeight));
    LayoutConstraints childConstraints = innerConstraints;
    childConstraints.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
    childConstraints.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();
    float maxW = 0.f;
    float maxH = 0.f;
    std::vector<Size> sizes{};
    sizes.reserve(stack.children.size());
    for (Element const& child : stack.children) {
      Size const size = measureElement(child, childConstraints, LayoutHints{});
      sizes.push_back(size);
      maxW = std::max(maxW, size.width);
      maxH = std::max(maxH, size.height);
    }
    if (innerW <= 0.f) {
      innerW = maxW;
    }
    if (innerH <= 0.f) {
      innerH = maxH;
    }
    innerW = std::max(innerW, maxW);
    innerH = std::max(innerH, maxH);

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(stack.children.size());
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      Element const& child = stack.children[i];
      LocalId const local = childLocalId(child, i);
      NodeId const childId = SceneTree::childId(id, local);
      std::unique_ptr<SceneNode> reuse{};
      if (auto it = reusable.find(childId); it != reusable.end()) {
        reuse = std::move(it->second);
        reusable.erase(it);
      }
      LayoutConstraints childBuild{};
      childBuild.maxWidth = innerW;
      childBuild.maxHeight = innerH;
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      pushFrame(childBuild, LayoutHints{}, contentOrigin, std::move(childKey));
      std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
      popFrame();
      childNode->position.x += hAlignOffset(childNode->bounds.width, innerW, stack.horizontalAlignment);
      childNode->position.y += vAlignOffset(childNode->bounds.height, innerH, stack.verticalAlignment);
      nextChildren.push_back(std::move(childNode));
    }
    group->replaceChildren(std::move(nextChildren));
    setGroupBounds(*group, Size{innerW, innerH});
    core = std::move(group);
    break;
  }
  case ElementType::Grid: {
    Grid const& grid = sceneEl.as<Grid>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(existing));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    std::vector<std::unique_ptr<SceneNode>> existingChildren = group->releaseChildren();
    std::unordered_map<NodeId, std::unique_ptr<SceneNode>, ::flux::NodeIdHash> reusable{};
    reusable.reserve(existingChildren.size());
    for (std::unique_ptr<SceneNode>& child : existingChildren) {
      if (child) {
        reusable.emplace(child->id(), std::move(child));
      }
    }

    float const innerW = std::max(0.f, assignedSpan(0.f, innerConstraints.maxWidth));
    float const innerH = std::max(0.f, assignedSpan(0.f, innerConstraints.maxHeight));
    std::size_t const cols = std::max<std::size_t>(1, grid.columns);
    std::size_t const n = grid.children.size();
    std::size_t const rowCount = n == 0 ? 0 : (n + cols - 1) / cols;
    float const cellW =
        innerW > 0.f ? std::max(0.f, (innerW - static_cast<float>(cols - 1) * grid.horizontalSpacing) /
                                        static_cast<float>(cols))
                     : 0.f;
    float const cellH =
        innerH > 0.f && rowCount > 0
            ? std::max(0.f, (innerH - static_cast<float>(rowCount - 1) * grid.verticalSpacing) /
                                 static_cast<float>(rowCount))
            : 0.f;

    LayoutConstraints childConstraints = innerConstraints;
    childConstraints.maxWidth = cellW > 0.f ? cellW : std::numeric_limits<float>::infinity();
    childConstraints.maxHeight = cellH > 0.f ? cellH : std::numeric_limits<float>::infinity();
    clampLayoutMinToMax(childConstraints);

    std::vector<Size> sizes{};
    sizes.reserve(n);
    for (Element const& child : grid.children) {
      sizes.push_back(measureElement(child, childConstraints, LayoutHints{}));
    }

    std::vector<float> rowH(rowCount, cellH);
    std::vector<float> colW(cols, cellW);
    if (cellH <= 0.f) {
      std::fill(rowH.begin(), rowH.end(), 0.f);
      for (std::size_t i = 0; i < n; ++i) {
        rowH[i / cols] = std::max(rowH[i / cols], sizes[i].height);
      }
    }
    if (cellW <= 0.f) {
      std::fill(colW.begin(), colW.end(), 0.f);
      for (std::size_t i = 0; i < n; ++i) {
        colW[i % cols] = std::max(colW[i % cols], sizes[i].width);
      }
    }

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(n);
    float y = 0.f;
    for (std::size_t row = 0; row < rowCount; ++row) {
      float x = 0.f;
      for (std::size_t col = 0; col < cols; ++col) {
        std::size_t const index = row * cols + col;
        if (index >= n) {
          break;
        }
        Element const& child = grid.children[index];
        LocalId const local = childLocalId(child, index);
        NodeId const childId = SceneTree::childId(id, local);
        std::unique_ptr<SceneNode> reuse{};
        if (auto it = reusable.find(childId); it != reusable.end()) {
          reuse = std::move(it->second);
          reusable.erase(it);
        }
        LayoutConstraints childBuild = childConstraints;
        if (cellW > 0.f) {
          childBuild.maxWidth = cellW;
        }
        if (rowH[row] > 0.f) {
          childBuild.maxHeight = rowH[row];
        }
        clampLayoutMinToMax(childBuild);
        ComponentKey childKey = current.key;
        childKey.push_back(local);
        pushFrame(childBuild, LayoutHints{}, Point{contentOrigin.x + x, contentOrigin.y + y}, std::move(childKey));
        std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
        popFrame();
        float const frameW = cellW > 0.f ? cellW : colW[col];
        float const frameH = rowH[row] > 0.f ? rowH[row] : sizes[index].height;
        childNode->position.x += x + hAlignOffset(childNode->bounds.width, frameW, grid.horizontalAlignment);
        childNode->position.y += y + vAlignOffset(childNode->bounds.height, frameH, grid.verticalAlignment);
        nextChildren.push_back(std::move(childNode));
        x += frameW;
        if (col + 1 < cols && index + 1 < n) {
          x += grid.horizontalSpacing;
        }
      }
      y += rowH[row];
      if (row + 1 < rowCount) {
        y += grid.verticalSpacing;
      }
    }
    group->replaceChildren(std::move(nextChildren));
    float totalW = innerW;
    if (totalW <= 0.f) {
      totalW = 0.f;
      std::size_t const usedCols = std::min(cols, n);
      if (usedCols > 1) {
        totalW += static_cast<float>(usedCols - 1) * grid.horizontalSpacing;
      }
      for (std::size_t col = 0; col < usedCols; ++col) {
        totalW += colW[col];
      }
    }
    float totalH = innerH;
    if (totalH <= 0.f) {
      totalH = 0.f;
      if (rowCount > 1) {
        totalH += static_cast<float>(rowCount - 1) * grid.verticalSpacing;
      }
      for (float rowHeight : rowH) {
        totalH += rowHeight;
      }
    }
    setGroupBounds(*group, Size{totalW, totalH});
    core = std::move(group);
    break;
  }
  case ElementType::OffsetView: {
    OffsetView const& offsetView = sceneEl.as<OffsetView>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(existing));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    std::vector<std::unique_ptr<SceneNode>> existingChildren = group->releaseChildren();
    std::unordered_map<NodeId, std::unique_ptr<SceneNode>, ::flux::NodeIdHash> reusable{};
    reusable.reserve(existingChildren.size());
    for (std::unique_ptr<SceneNode>& child : existingChildren) {
      if (child) {
        reusable.emplace(child->id(), std::move(child));
      }
    }

    Size const viewport{
        std::max(0.f, assignedSpan(0.f, innerConstraints.maxWidth)),
        std::max(0.f, assignedSpan(0.f, innerConstraints.maxHeight)),
    };
    if (offsetView.viewportSize.signal && !sizeApproximatelyEqual(*offsetView.viewportSize, viewport)) {
      offsetView.viewportSize.setSilently(viewport);
    }
    LayoutConstraints childConstraints = scrollChildConstraints(offsetView.axis, innerConstraints, viewport);
    std::vector<Size> sizes{};
    sizes.reserve(offsetView.children.size());
    for (Element const& child : offsetView.children) {
      sizes.push_back(measureElement(child, childConstraints, LayoutHints{}));
    }
    Size const contentSize = scrollContentSize(offsetView.axis, sizes);
    if (offsetView.contentSize.signal && !sizeApproximatelyEqual(*offsetView.contentSize, contentSize)) {
      offsetView.contentSize.setSilently(contentSize);
    }

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(offsetView.children.size());
    if (offsetView.axis == ScrollAxis::Horizontal) {
      float x = -offsetView.offset.x;
      for (std::size_t i = 0; i < offsetView.children.size(); ++i) {
        Element const& child = offsetView.children[i];
        LocalId const local = childLocalId(child, i);
        NodeId const childId = SceneTree::childId(id, local);
        std::unique_ptr<SceneNode> reuse{};
        if (auto it = reusable.find(childId); it != reusable.end()) {
          reuse = std::move(it->second);
          reusable.erase(it);
        }
        LayoutConstraints childBuild = childConstraints;
        ComponentKey childKey = current.key;
        childKey.push_back(local);
        pushFrame(childBuild, LayoutHints{}, Point{contentOrigin.x + x, contentOrigin.y}, std::move(childKey));
        std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
        popFrame();
        childNode->position.x += x;
        nextChildren.push_back(std::move(childNode));
        x += sizes[i].width;
      }
    } else if (offsetView.axis == ScrollAxis::Vertical) {
      float y = -offsetView.offset.y;
      for (std::size_t i = 0; i < offsetView.children.size(); ++i) {
        Element const& child = offsetView.children[i];
        LocalId const local = childLocalId(child, i);
        NodeId const childId = SceneTree::childId(id, local);
        std::unique_ptr<SceneNode> reuse{};
        if (auto it = reusable.find(childId); it != reusable.end()) {
          reuse = std::move(it->second);
          reusable.erase(it);
        }
        LayoutConstraints childBuild = childConstraints;
        ComponentKey childKey = current.key;
        childKey.push_back(local);
        pushFrame(childBuild, LayoutHints{}, Point{contentOrigin.x, contentOrigin.y + y}, std::move(childKey));
        std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
        popFrame();
        childNode->position.y += y;
        nextChildren.push_back(std::move(childNode));
        y += sizes[i].height;
      }
    } else {
      for (std::size_t i = 0; i < offsetView.children.size(); ++i) {
        Element const& child = offsetView.children[i];
        LocalId const local = childLocalId(child, i);
        NodeId const childId = SceneTree::childId(id, local);
        std::unique_ptr<SceneNode> reuse{};
        if (auto it = reusable.find(childId); it != reusable.end()) {
          reuse = std::move(it->second);
          reusable.erase(it);
        }
        LayoutConstraints childBuild = childConstraints;
        ComponentKey childKey = current.key;
        childKey.push_back(local);
        pushFrame(childBuild, LayoutHints{},
                  Point{contentOrigin.x - offsetView.offset.x, contentOrigin.y - offsetView.offset.y},
                  std::move(childKey));
        std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
        popFrame();
        childNode->position.x -= offsetView.offset.x;
        childNode->position.y -= offsetView.offset.y;
        nextChildren.push_back(std::move(childNode));
      }
    }
    group->replaceChildren(std::move(nextChildren));
    setGroupBounds(*group, contentSize);
    core = std::move(group);
    break;
  }
  case ElementType::ScrollView: {
    ScrollView const& scrollView = sceneEl.as<ScrollView>();
    Point scrollOffset = scrollView.scrollOffset.signal ? *scrollView.scrollOffset : Point{};
    Size const viewport = outerSize;
    if (scrollView.viewportSize.signal && !sizeApproximatelyEqual(*scrollView.viewportSize, viewport)) {
      scrollView.viewportSize.setSilently(viewport);
    }
    LayoutConstraints childConstraints = scrollChildConstraints(scrollView.axis, innerConstraints, viewport);

    std::vector<Element> contentChildren = scrollView.children;
    std::vector<Size> sizes{};
    sizes.reserve(contentChildren.size());
    for (Element const& child : contentChildren) {
      sizes.push_back(measureElement(child, childConstraints, LayoutHints{}));
    }
    Size const contentSize = scrollContentSize(scrollView.axis, sizes);
    if (scrollView.contentSize.signal && !sizeApproximatelyEqual(*scrollView.contentSize, contentSize)) {
      scrollView.contentSize.setSilently(contentSize);
    }
    scrollOffset = clampScrollOffset(scrollView.axis, scrollOffset, viewport, contentSize);

    std::unique_ptr<ModifierSceneNode> modifier = releaseAs<ModifierSceneNode>(std::move(existing));
    if (!modifier) {
      modifier = std::make_unique<ModifierSceneNode>(id);
    }
    std::unique_ptr<SceneNode> existingContent{};
    if (!modifier->children().empty()) {
      std::vector<std::unique_ptr<SceneNode>> children = modifier->releaseChildren();
      if (!children.empty()) {
        existingContent = std::move(children.front());
      }
    }
    std::unique_ptr<SceneNode> contentGroup = releasePlainGroup(std::move(existingContent));
    if (!contentGroup) {
      contentGroup = std::make_unique<SceneNode>(SceneTree::childId(id, LocalId::fromString("$content")));
    }
    std::vector<std::unique_ptr<SceneNode>> existingChildren = contentGroup->releaseChildren();
    std::unordered_map<NodeId, std::unique_ptr<SceneNode>, ::flux::NodeIdHash> reusable{};
    reusable.reserve(existingChildren.size());
    for (std::unique_ptr<SceneNode>& child : existingChildren) {
      if (child) {
        reusable.emplace(child->id(), std::move(child));
      }
    }

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(contentChildren.size());
    if (scrollView.axis == ScrollAxis::Horizontal) {
      float x = -scrollOffset.x;
      for (std::size_t i = 0; i < contentChildren.size(); ++i) {
        Element const& child = contentChildren[i];
        LocalId const local = childLocalId(child, i);
        NodeId const childId = SceneTree::childId(contentGroup->id(), local);
        std::unique_ptr<SceneNode> reuse{};
        if (auto it = reusable.find(childId); it != reusable.end()) {
          reuse = std::move(it->second);
          reusable.erase(it);
        }
        ComponentKey childKey = current.key;
        childKey.push_back(local);
        pushFrame(childConstraints, LayoutHints{}, Point{contentOrigin.x + x, contentOrigin.y}, std::move(childKey));
        std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
        popFrame();
        childNode->position.x += x;
        nextChildren.push_back(std::move(childNode));
        x += sizes[i].width;
      }
    } else if (scrollView.axis == ScrollAxis::Vertical) {
      float y = -scrollOffset.y;
      for (std::size_t i = 0; i < contentChildren.size(); ++i) {
        Element const& child = contentChildren[i];
        LocalId const local = childLocalId(child, i);
        NodeId const childId = SceneTree::childId(contentGroup->id(), local);
        std::unique_ptr<SceneNode> reuse{};
        if (auto it = reusable.find(childId); it != reusable.end()) {
          reuse = std::move(it->second);
          reusable.erase(it);
        }
        ComponentKey childKey = current.key;
        childKey.push_back(local);
        pushFrame(childConstraints, LayoutHints{}, Point{contentOrigin.x, contentOrigin.y + y}, std::move(childKey));
        std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
        popFrame();
        childNode->position.y += y;
        nextChildren.push_back(std::move(childNode));
        y += sizes[i].height;
      }
    } else {
      for (std::size_t i = 0; i < contentChildren.size(); ++i) {
        Element const& child = contentChildren[i];
        LocalId const local = childLocalId(child, i);
        NodeId const childId = SceneTree::childId(contentGroup->id(), local);
        std::unique_ptr<SceneNode> reuse{};
        if (auto it = reusable.find(childId); it != reusable.end()) {
          reuse = std::move(it->second);
          reusable.erase(it);
        }
        ComponentKey childKey = current.key;
        childKey.push_back(local);
        pushFrame(childConstraints, LayoutHints{},
                  Point{contentOrigin.x - scrollOffset.x, contentOrigin.y - scrollOffset.y}, std::move(childKey));
        std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
        popFrame();
        childNode->position.x -= scrollOffset.x;
        childNode->position.y -= scrollOffset.y;
        nextChildren.push_back(std::move(childNode));
      }
    }
    contentGroup->replaceChildren(std::move(nextChildren));
    setGroupBounds(*contentGroup, contentSize);
    modifier->replaceChildren({});
    modifier->appendChild(std::move(contentGroup));
    modifier->clip = Rect{0.f, 0.f, viewport.width, viewport.height};
    modifier->opacity = 1.f;
    modifier->fill = FillStyle::none();
    modifier->stroke = StrokeStyle::none();
    modifier->shadow = ShadowStyle::none();
    modifier->cornerRadius = {};
    modifier->recomputeBounds();
    core = std::move(modifier);
    break;
  }
  case ElementType::ScaleAroundCenter: {
    ScaleAroundCenter const& scaled = sceneEl.as<ScaleAroundCenter>();
    std::unique_ptr<CustomTransformSceneNode> transformNode = releaseAs<CustomTransformSceneNode>(std::move(existing));
    if (!transformNode) {
      transformNode = std::make_unique<CustomTransformSceneNode>(id);
    }
    std::unique_ptr<SceneNode> existingChild{};
    if (!transformNode->children().empty()) {
      std::vector<std::unique_ptr<SceneNode>> children = transformNode->releaseChildren();
      if (!children.empty()) {
        existingChild = std::move(children.front());
      }
    }
    Size const childSize = measureElement(scaled.child, innerConstraints, LayoutHints{});
    Point const pivot{childSize.width * 0.5f, childSize.height * 0.5f};
    ComponentKey childKey = current.key;
    childKey.push_back(LocalId::fromString("$child"));
    pushFrame(innerConstraints, LayoutHints{}, contentOrigin, childKey);
    std::unique_ptr<SceneNode> childNode =
        buildOrReuse(scaled.child, SceneTree::childId(id, LocalId::fromString("$child")), std::move(existingChild));
    popFrame();
    childNode->position = {};
    transformNode->replaceChildren({});
    transformNode->appendChild(std::move(childNode));
    transformNode->transform =
        Mat3::translate(pivot) * Mat3::scale(scaled.scale) * Mat3::translate(Point{-pivot.x, -pivot.y});
    transformNode->recomputeBounds();
    core = std::move(transformNode);
    break;
  }
  case ElementType::Spacer:
    core = std::make_unique<SceneNode>(id);
    core->bounds = sizeRect(paddedContentSize);
    break;
  default:
    core = std::make_unique<SceneNode>(id);
    core->bounds = sizeRect(paddedContentSize);
    break;
  }

  Size const contentSize = rectSize(core->bounds);
  std::unique_ptr<SceneNode> root = std::move(core);
  root->position = {};
  bool const leafOwnsModifierPaint = el.leafDrawsFillStrokeShadowFromModifiers();

  if (needsLayoutWrapper(mods, outerSize, contentSize)) {
    std::unique_ptr<SceneNode> wrapper = std::make_unique<SceneNode>(id);
    std::vector<std::unique_ptr<SceneNode>> children{};
    children.reserve(hasOverlay(mods) ? 2 : 1);
    root->position = Point{std::max(0.f, padding.left), std::max(0.f, padding.top)};
    children.push_back(std::move(root));
    if (hasOverlay(mods)) {
      LayoutConstraints overlayConstraints = current.constraints;
      overlayConstraints.maxWidth =
          outerSize.width > 0.f ? outerSize.width : std::numeric_limits<float>::infinity();
      overlayConstraints.maxHeight =
          outerSize.height > 0.f ? outerSize.height : std::numeric_limits<float>::infinity();
      ComponentKey overlayKey = current.key;
      overlayKey.push_back(LocalId::fromString("$overlay"));
      pushFrame(overlayConstraints, current.hints, Point{current.origin.x + subtreeOffset.x, current.origin.y + subtreeOffset.y},
                std::move(overlayKey));
      std::unique_ptr<SceneNode> overlayNode =
          buildOrReuse(*mods->overlay, SceneTree::childId(id, LocalId::fromString("$overlay")), nullptr);
      popFrame();
      children.push_back(std::move(overlayNode));
    }
    wrapper->replaceChildren(std::move(children));
    setGroupBounds(*wrapper, outerSize);
    root = std::move(wrapper);
  }

  if (needsModifierWrapper(mods, leafOwnsModifierPaint)) {
    std::unique_ptr<ModifierSceneNode> wrapper = releaseAs<ModifierSceneNode>(std::move(existing));
    if (!wrapper) {
      wrapper = std::make_unique<ModifierSceneNode>(id);
    }
    wrapper->replaceChildren({});
    root->position = {};
    wrapper->appendChild(std::move(root));
    wrapper->clip = mods->clip ? std::optional<Rect>(Rect{0.f, 0.f, outerSize.width, outerSize.height}) : std::nullopt;
    wrapper->opacity = mods->opacity;
    wrapper->fill = leafOwnsModifierPaint ? FillStyle::none() : mods->fill;
    wrapper->stroke = leafOwnsModifierPaint ? StrokeStyle::none() : mods->stroke;
    wrapper->shadow = leafOwnsModifierPaint ? ShadowStyle::none() : mods->shadow;
    wrapper->cornerRadius = leafOwnsModifierPaint ? CornerRadius{} : mods->cornerRadius;
    wrapper->recomputeBounds();
    root = std::move(wrapper);
  }

  if (std::unique_ptr<InteractionData> interaction = makeInteractionData(mods, current.key)) {
    root->setInteraction(std::move(interaction));
  } else {
    root->setInteraction(nullptr);
  }

  root->position = subtreeOffset;
  if (root->bounds.width <= 0.f && root->bounds.height <= 0.f) {
    root->bounds = sizeRect(outerSize);
  }
  recordGeometry(outerSize.width > 0.f || outerSize.height > 0.f ? outerSize : rectSize(root->bounds));
  return root;
}

} // namespace flux
