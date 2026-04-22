#include <Flux/UI/SceneBuilder.hpp>

#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Animation.hpp>
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
#include <Flux/UI/MeasureContext.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Grid.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Image.hpp>
#include <Flux/UI/Views/Line.hpp>
#include <Flux/UI/Views/OffsetView.hpp>
#include <Flux/UI/Views/PathShape.hpp>
#include <Flux/UI/Views/PopoverCalloutPath.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Render.hpp>
#include <Flux/UI/Views/ScaleAroundCenter.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/SelectableTextSupport.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextEditUtils.hpp>
#include <Flux/UI/Views/TextSupport.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include "Scene/SceneGeometry.hpp"
#include "UI/Layout/Algorithms/GridLayout.hpp"
#include "UI/Layout/Algorithms/OverlayLayout.hpp"
#include "UI/Layout/Algorithms/ScrollLayout.hpp"
#include "UI/Layout/Algorithms/StackLayout.hpp"
#include "UI/Layout/LayoutHelpers.hpp"
#include "UI/SceneBuilder/NodeReuse.hpp"

namespace flux {

namespace {

using layout::assignedSpan;
using layout::clampLayoutMinToMax;
using layout::hAlignOffset;
using layout::stackMainAxisSpan;
using layout::vAlignOffset;
using layout::warnFlexGrowIfParentMainAxisUnconstrained;

constexpr float kApproxEpsilon = 1e-4f;
constexpr std::int8_t kUnsetAlignmentStamp = -1;

bool nearlyEqual(float lhs, float rhs, float eps = kApproxEpsilon) {
  return std::fabs(lhs - rhs) <= eps;
}

bool nearlyEqual(Size lhs, Size rhs) {
  return nearlyEqual(lhs.width, rhs.width) && nearlyEqual(lhs.height, rhs.height);
}

bool zStackAxisStretches(std::optional<Alignment> alignment) {
  return !alignment || *alignment == Alignment::Stretch;
}

float resolvedAssignedSpan(float assignedSpan, bool hasAssignedSpan, float fallbackSpan) {
  if (hasAssignedSpan) {
    return std::max(0.f, assignedSpan);
  }
  if (std::isfinite(fallbackSpan) && fallbackSpan > 0.f) {
    return fallbackSpan;
  }
  return 0.f;
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

bool isTransparentModifierWrapper(SceneNode const& node) {
  if (node.kind() != SceneNodeKind::Modifier || typeid(node) != typeid(ModifierSceneNode)) {
    return false;
  }
  return node.children().size() == 1 && node.children()[0] && node.children()[0]->id() == node.id();
}

bool isTransparentLayoutWrapper(SceneNode const& node) {
  return isPlainGroup(node) && !node.children().empty() && node.children()[0] &&
         node.children()[0]->id() == node.id();
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

std::unique_ptr<ModifierSceneNode> takeTransparentModifierWrapper(std::unique_ptr<SceneNode>& node) {
  if (!node || !isTransparentModifierWrapper(*node)) {
    return nullptr;
  }
  auto wrapper = std::unique_ptr<ModifierSceneNode>(static_cast<ModifierSceneNode*>(node.release()));
  std::vector<std::unique_ptr<SceneNode>> children = wrapper->releaseChildren();
  node = children.empty() ? nullptr : std::move(children[0]);
  return wrapper;
}

struct LayoutWrapperReuse {
  std::unique_ptr<SceneNode> wrapper{};
  std::unique_ptr<SceneNode> overlay{};
};

LayoutWrapperReuse takeTransparentLayoutWrapper(std::unique_ptr<SceneNode>& node) {
  if (!node || !isTransparentLayoutWrapper(*node)) {
    return {};
  }

  LayoutWrapperReuse reuse{};
  reuse.wrapper = std::move(node);
  std::vector<std::unique_ptr<SceneNode>> children = reuse.wrapper->releaseChildren();
  node = children.empty() ? nullptr : std::move(children[0]);

  NodeId const overlayId = SceneTree::childId(reuse.wrapper->id(), LocalId::fromString("$overlay"));
  for (std::size_t i = 1; i < children.size(); ++i) {
    if (children[i] && children[i]->id() == overlayId) {
      reuse.overlay = std::move(children[i]);
      break;
    }
  }
  return reuse;
}

void setGroupBounds(SceneNode& node, Size minSize = {}) {
  Rect bounds = sizeRect(minSize);
  for (std::unique_ptr<SceneNode> const& child : node.children()) {
    bounds = scene::unionRect(bounds, scene::offsetRect(child->bounds, child->position));
  }
  node.bounds = bounds;
  node.markBoundsDirty();
}

void setAssignedGroupBounds(SceneNode& node, Size size) {
  node.bounds = sizeRect(size);
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

Point modifierOffset(detail::ElementModifiers const* mods) {
  if (!mods) {
    return {};
  }
  return Point{mods->positionX + mods->translation.x, mods->positionY + mods->translation.y};
}

bool needsModifierWrapper(detail::ElementModifiers const* mods, bool leafOwnsPaint) {
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

bool hasOverlay(detail::ElementModifiers const* mods) {
  return mods && mods->overlay != nullptr;
}

bool hasPadding(detail::ElementModifiers const* mods) {
  return mods && !mods->padding.isZero();
}

bool needsLayoutWrapper(detail::ElementModifiers const* mods, Size outerSize, Size contentSize) {
  if (!mods) {
    return false;
  }
  if (hasPadding(mods) || hasOverlay(mods)) {
    return true;
  }
  return !nearlyEqual(outerSize, contentSize);
}

bool needsDecorationPass(detail::ElementModifiers const* mods, Size outerSize, Size contentSize,
                         bool leafOwnsPaint, InteractionData const* interaction) {
  if (!mods) {
    return interaction != nullptr;
  }
  if (needsLayoutWrapper(mods, outerSize, contentSize) || needsModifierWrapper(mods, leafOwnsPaint)) {
    return true;
  }
  if (interaction != nullptr) {
    return true;
  }
  Point const offset = modifierOffset(mods);
  return std::fabs(offset.x) > 1e-6f || std::fabs(offset.y) > 1e-6f;
}

Rect chromeRectForSize(Size size) {
  return Rect{0.f, 0.f, std::max(0.f, size.width), std::max(0.f, size.height)};
}

ComponentKey directCompositeBodyKey(ComponentKey const& parentKey) {
  ComponentKey key = parentKey;
  key.push_back(LocalId::fromIndex(0));
  return key;
}

bool rectIsMeaningful(Rect rect) {
  return rect.width > 0.f || rect.height > 0.f;
}

bool canUseResolvedBodyForRetainedStamp(Element const& el, Element const& sceneEl) {
  return &sceneEl != &el && el.isComposite() && el.typeTag() == ElementType::Unknown &&
         !el.modifiers() && !el.environmentLayer();
}

Element const& retainedStampElement(Element const& el, Element const& sceneEl) {
  return canUseResolvedBodyForRetainedStamp(el, sceneEl) ? sceneEl : el;
}

std::unique_ptr<InteractionData> makeInteractionData(detail::ElementModifiers const* mods,
                                                     ComponentKey const& key) {
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

Theme const& activeTheme(EnvironmentStack const& environment) {
  if (Theme const* theme = environment.find<Theme>()) {
    return *theme;
  }
  static Theme const fallback = Theme::light();
  return fallback;
}

FillStyle resolveFillStyle(FillStyle const& style, Theme const& theme) {
  FillStyle resolved = style;
  Color color{};
  if (resolved.solidColor(&color)) {
    resolved.data = resolveColor(color, theme);
  }
  return resolved;
}

StrokeStyle resolveStrokeStyle(StrokeStyle const& style, Theme const& theme) {
  StrokeStyle resolved = style;
  if (resolved.type == StrokeStyle::Type::Solid) {
    resolved.color = resolveColor(resolved.color, theme);
  }
  return resolved;
}

ShadowStyle resolveShadowStyle(ShadowStyle const& style, Theme const& theme) {
  ShadowStyle resolved = style;
  resolved.color = resolveColor(resolved.color, theme);
  return resolved;
}

bool updateIfChanged(Size& field, Size value);
bool updateIfChanged(Point& field, Point value);
template<typename T>
bool updateIfChanged(T& field, T const& value);
bool updateIfChanged(Font& field, Font const& value);

bool configureTextSceneNode(TextSceneNode& textNode, TextSystem& textSystem, Text const& text, Font const& resolvedFont,
                            Color const& resolvedColor, Rect frameRect, std::string const& displayText,
                            std::shared_ptr<TextLayout const> const& layout) {
  bool dirty = false;
  dirty |= updateIfChanged(textNode.text, displayText);
  dirty |= updateIfChanged(textNode.font, resolvedFont);
  dirty |= updateIfChanged(textNode.color, resolvedColor);
  dirty |= updateIfChanged(textNode.horizontalAlignment, text.horizontalAlignment);
  dirty |= updateIfChanged(textNode.verticalAlignment, text.verticalAlignment);
  dirty |= updateIfChanged(textNode.wrapping, text.wrapping);
  dirty |= updateIfChanged(textNode.maxLines, text.maxLines);
  dirty |= updateIfChanged(textNode.firstBaselineOffset, text.firstBaselineOffset);
  dirty |= updateIfChanged(textNode.widthConstraint, text.wrapping == TextWrapping::NoWrap ? 0.f : frameRect.width);
  dirty |= updateIfChanged(textNode.origin, Point{0.f, 0.f});
  dirty |= updateIfChanged(textNode.allocation, Rect{0.f, 0.f, frameRect.width, frameRect.height});
  dirty |= updateIfChanged(textNode.layout, layout);
  if (textNode.textSystem != &textSystem) {
    textNode.textSystem = &textSystem;
    dirty = true;
  }
  if (dirty) {
    textNode.invalidatePaint();
    textNode.markBoundsDirty();
  }
  textNode.position = {};
  textNode.recomputeBounds();
  return dirty;
}

std::unique_ptr<InteractionData>
makeSelectableTextInteraction(detail::ElementModifiers const* mods, ComponentKey const& key,
                              std::shared_ptr<detail::SelectableTextState> const& state) {
  if (!state) {
    return makeInteractionData(mods, key);
  }
  std::unique_ptr<InteractionData> data = makeInteractionData(mods, key);
  if (!data) {
    data = std::make_unique<InteractionData>();
  }
  data->stableTargetKey = key;
  data->focusable = true;
  data->cursor = Cursor::IBeam;
  data->onPointerDown = [state](Point local) { detail::handleSelectableTextPointerDown(*state, local); };
  data->onPointerMove = [state](Point local) { detail::handleSelectableTextPointerDrag(*state, local); };
  data->onPointerUp = [state](Point) { detail::handleSelectableTextPointerUp(*state); };
  data->onKeyDown = [state](KeyCode keyCode, Modifiers modifiers) {
    detail::handleSelectableTextKey(*state, keyCode, modifiers);
  };
  return data;
}

LocalId childLocalId(Element const& child, std::size_t index) {
  if (child.explicitKey()) {
    return LocalId::fromString(*child.explicitKey());
  }
  return LocalId::fromIndex(index);
}

Rect assignedFrameForLeaf(Size measuredSize, LayoutConstraints const& constraints, Size assignedSize,
                          bool hasAssignedWidth, bool hasAssignedHeight, detail::ElementModifiers const* mods,
                          LayoutHints const& hints) {
  Rect explicitBox{};
  if (mods) {
    explicitBox.width = std::max(0.f, mods->sizeWidth);
    explicitBox.height = std::max(0.f, mods->sizeHeight);
  }
  float slotWidth = 0.f;
  float slotHeight = 0.f;
  bool useAssignedWidth = false;
  bool useAssignedHeight = false;
  if (hasAssignedWidth) {
    slotWidth = std::max(0.f, assignedSize.width);
    useAssignedWidth = true;
  } else if (std::isfinite(constraints.maxWidth)) {
    slotWidth = std::max(0.f, constraints.maxWidth);
    useAssignedWidth = true;
  } else if (std::isfinite(constraints.minWidth) && constraints.minWidth > 0.f) {
    slotWidth = constraints.minWidth;
    useAssignedWidth = true;
  }
  if (hasAssignedHeight) {
    slotHeight = std::max(0.f, assignedSize.height);
    useAssignedHeight = true;
  } else if (std::isfinite(constraints.maxHeight)) {
    slotHeight = std::max(0.f, constraints.maxHeight);
    useAssignedHeight = true;
  } else if (std::isfinite(constraints.minHeight) && constraints.minHeight > 0.f) {
    slotHeight = constraints.minHeight;
    useAssignedHeight = true;
  }
  Rect const childFrame{
      0.f,
      0.f,
      useAssignedWidth ? slotWidth : measuredSize.width,
      useAssignedHeight ? slotHeight : measuredSize.height,
  };
  return detail::resolveLeafLayoutBounds(explicitBox, childFrame, constraints, hints);
}

Size assignedOuterSizeForFrame(Size measuredSize, LayoutConstraints const& constraints, Size assignedSize,
                               bool hasAssignedWidth, bool hasAssignedHeight,
                               detail::ElementModifiers const* mods = nullptr) {
  Size size = measuredSize;
  bool const explicitWidth = mods && mods->sizeWidth > 0.f;
  bool const explicitHeight = mods && mods->sizeHeight > 0.f;
  if (!explicitWidth && hasAssignedWidth) {
    size.width = std::max(0.f, assignedSize.width);
  } else if (!explicitWidth && std::isfinite(constraints.maxWidth) && constraints.maxWidth > 0.f) {
    size.width = std::max(0.f, constraints.maxWidth);
  } else if (!explicitWidth && constraints.minWidth > 0.f) {
    size.width = std::max(size.width, constraints.minWidth);
  }

  if (!explicitHeight && hasAssignedHeight) {
    size.height = std::max(0.f, assignedSize.height);
  } else if (!explicitHeight && std::isfinite(constraints.maxHeight) && constraints.maxHeight > 0.f) {
    size.height = std::max(0.f, constraints.maxHeight);
  } else if (!explicitHeight && constraints.minHeight > 0.f) {
    size.height = std::max(size.height, constraints.minHeight);
  }
  return size;
}

bool textUsesContentBox(detail::ElementModifiers const* mods) {
  if (!mods) {
    return false;
  }
  return !mods->padding.isZero() || !mods->fill.isNone() || !mods->stroke.isNone() ||
         !mods->shadow.isNone() || !mods->cornerRadius.isZero() || mods->opacity < 1.f - 1e-6f ||
         std::fabs(mods->translation.x) > 1e-6f || std::fabs(mods->translation.y) > 1e-6f ||
         mods->clip || std::fabs(mods->positionX) > 1e-6f || std::fabs(mods->positionY) > 1e-6f ||
         mods->sizeWidth > 0.f || mods->sizeHeight > 0.f || mods->overlay != nullptr;
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

bool sizeApproximatelyEqual(Size lhs, Size rhs) {
  return nearlyEqual(lhs.width, rhs.width, 0.5f) && nearlyEqual(lhs.height, rhs.height, 0.5f);
}

std::int8_t encodeAlignmentStamp(std::optional<Alignment> alignment) {
  if (!alignment) {
    return kUnsetAlignmentStamp;
  }
  return static_cast<std::int8_t>(static_cast<int>(*alignment));
}

Color scrollIndicatorColorForTheme(Theme const& theme) {
  return Color{
      theme.secondaryLabelColor.r,
      theme.secondaryLabelColor.g,
      theme.secondaryLabelColor.b,
      0.55f,
  };
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
                             ComponentKey key, Size assignedSize, bool hasAssignedWidth,
                             bool hasAssignedHeight) {
  frames_.push_back(FrameState{
      .constraints = constraints,
      .hints = hints,
      .origin = origin,
      .assignedSize = assignedSize,
      .hasAssignedWidth = hasAssignedWidth,
      .hasAssignedHeight = hasAssignedHeight,
      .key = std::move(key),
  });
}

void SceneBuilder::popFrame() {
  frames_.pop_back();
}

bool SceneBuilder::canRetainExistingSubtree(Element const& el, Element const& sceneEl,
                                            SceneNode const& existing) const {
  StateStore* const store = StateStore::current();
  if (!store || store->hasDirtyDescendant(frame().key)) {
    return false;
  }

  Element const& retainedStampEl = retainedStampElement(el, sceneEl);
  RetainedBuildStamp const& stamp = existing.retainedBuildStamp();
  FrameState const& current = frame();
  if (stamp.measureId == 0 || stamp.measureId != retainedStampEl.measureId()) {
    return false;
  }
  if (stamp.maxWidth != current.constraints.maxWidth || stamp.maxHeight != current.constraints.maxHeight ||
      stamp.minWidth != current.constraints.minWidth || stamp.minHeight != current.constraints.minHeight) {
    return false;
  }
  if (stamp.assignedWidth != current.assignedSize.width || stamp.assignedHeight != current.assignedSize.height ||
      stamp.hasAssignedWidth != current.hasAssignedWidth || stamp.hasAssignedHeight != current.hasAssignedHeight) {
    return false;
  }
  return stamp.hStackCrossAlign == encodeAlignmentStamp(current.hints.hStackCrossAlign) &&
         stamp.vStackCrossAlign == encodeAlignmentStamp(current.hints.vStackCrossAlign) &&
         stamp.zStackHorizontalAlign == encodeAlignmentStamp(current.hints.zStackHorizontalAlign) &&
         stamp.zStackVerticalAlign == encodeAlignmentStamp(current.hints.zStackVerticalAlign);
}

void SceneBuilder::stampRetainedBuild(SceneNode& node, Element const& el, Element const& sceneEl) const {
  FrameState const& current = frame();
  Element const& retainedStampEl = retainedStampElement(el, sceneEl);
  node.setRetainedBuildStamp(RetainedBuildStamp{
      .measureId = retainedStampEl.measureId(),
      .maxWidth = current.constraints.maxWidth,
      .maxHeight = current.constraints.maxHeight,
      .minWidth = current.constraints.minWidth,
      .minHeight = current.constraints.minHeight,
      .assignedWidth = current.assignedSize.width,
      .assignedHeight = current.assignedSize.height,
      .hasAssignedWidth = current.hasAssignedWidth,
      .hasAssignedHeight = current.hasAssignedHeight,
      .hStackCrossAlign = encodeAlignmentStamp(current.hints.hStackCrossAlign),
      .vStackCrossAlign = encodeAlignmentStamp(current.hints.vStackCrossAlign),
      .zStackHorizontalAlign = encodeAlignmentStamp(current.hints.zStackHorizontalAlign),
      .zStackVerticalAlign = encodeAlignmentStamp(current.hints.zStackVerticalAlign),
      .localPosition = node.position,
  });
}

Size SceneBuilder::measureElement(Element const& el, LayoutConstraints const& constraints,
                                  LayoutHints const& hints, ComponentKey const& key) const {
  MeasureContext measureContext{textSystem_};
  measureContext.pushConstraints(constraints, hints);
  measureContext.resetTraversalState(key);
  if (el.isComposite() && el.typeTag() == ElementType::Unknown) {
    measureContext.setMeasurementRootKey(key);
  } else {
    measureContext.clearMeasurementRootKey();
  }
  measureContext.setCurrentElement(&el);
  return el.measure(measureContext, constraints, hints, textSystem_);
}

std::unique_ptr<SceneNode>
SceneBuilder::decorateNode(std::unique_ptr<SceneNode> root, Element const& el,
                           detail::ElementModifiers const* mods,
                           std::unique_ptr<ModifierSceneNode> existingModifierWrapper,
                           std::unique_ptr<SceneNode> existingLayoutWrapper,
                           std::unique_ptr<SceneNode> existingOverlay, Size layoutOuterSize, Size outerSize,
                           Point subtreeOffset, EdgeInsets const& padding,
                           std::unique_ptr<InteractionData> interaction) {
  if (!root) {
    return nullptr;
  }

  FrameState const current = frame();
  Size const contentSize = rectSize(root->bounds);
  bool const leafOwnsModifierPaint = el.leafDrawsFillStrokeShadowFromModifiers();
  root->position = {};

  if (needsLayoutWrapper(mods, layoutOuterSize, contentSize)) {
    std::unique_ptr<SceneNode> wrapper = releasePlainGroup(std::move(existingLayoutWrapper));
    if (!wrapper) {
      wrapper = std::make_unique<SceneNode>(root->id());
    }
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
      pushFrame(overlayConstraints, current.hints,
                Point{current.origin.x + subtreeOffset.x, current.origin.y + subtreeOffset.y},
                std::move(overlayKey), layoutOuterSize, layoutOuterSize.width > 0.f,
                layoutOuterSize.height > 0.f);
      std::unique_ptr<SceneNode> overlayNode =
          buildOrReuse(*mods->overlay, SceneTree::childId(wrapper->id(), LocalId::fromString("$overlay")),
                       std::move(existingOverlay));
      popFrame();
      children.push_back(std::move(overlayNode));
    }
    wrapper->replaceChildren(std::move(children));
    setAssignedGroupBounds(*wrapper, layoutOuterSize);
    root = std::move(wrapper);
  }

  if (needsModifierWrapper(mods, leafOwnsModifierPaint)) {
    std::unique_ptr<ModifierSceneNode> wrapper = std::move(existingModifierWrapper);
    if (!wrapper) {
      wrapper = std::make_unique<ModifierSceneNode>(root->id());
    }
    wrapper->replaceChildren({});
    root->position = {};
    wrapper->appendChild(std::move(root));

    std::optional<Rect> const nextClip =
        mods->clip ? std::optional<Rect>(chromeRectForSize(outerSize)) : std::nullopt;
    Theme const& theme = activeTheme(environment_);
    FillStyle const nextFill = leafOwnsModifierPaint ? FillStyle::none() : resolveFillStyle(mods->fill, theme);
    StrokeStyle const nextStroke =
        leafOwnsModifierPaint ? StrokeStyle::none() : resolveStrokeStyle(mods->stroke, theme);
    ShadowStyle const nextShadow =
        leafOwnsModifierPaint ? ShadowStyle::none() : resolveShadowStyle(mods->shadow, theme);
    CornerRadius const nextCornerRadius = leafOwnsModifierPaint ? CornerRadius{} : mods->cornerRadius;
    Rect const nextChromeRect =
        !leafOwnsModifierPaint && rectIsMeaningful(chromeRectForSize(layoutOuterSize))
            ? chromeRectForSize(layoutOuterSize)
            : Rect{};

    bool const paintChanged = wrapper->chromeRect != nextChromeRect || wrapper->fill != nextFill ||
                              wrapper->stroke != nextStroke || wrapper->shadow != nextShadow ||
                              wrapper->cornerRadius != nextCornerRadius;
    bool const boundsChanged =
        wrapper->clip != nextClip || wrapper->opacity != mods->opacity || paintChanged;

    wrapper->chromeRect = nextChromeRect;
    wrapper->clip = nextClip;
    wrapper->opacity = mods->opacity;
    wrapper->fill = nextFill;
    wrapper->stroke = nextStroke;
    wrapper->shadow = nextShadow;
    wrapper->cornerRadius = nextCornerRadius;
    if (paintChanged) {
      wrapper->invalidatePaint();
    }
    if (boundsChanged) {
      wrapper->markBoundsDirty();
    }
    wrapper->recomputeBounds();
    root = std::move(wrapper);
  }

  root->setInteraction(std::move(interaction));
  root->position = subtreeOffset;
  return root;
}

std::unique_ptr<SceneNode> SceneBuilder::build(Element const& el, NodeId id,
                                               LayoutConstraints const& constraints,
                                               std::unique_ptr<SceneNode> existing,
                                               ComponentKey rootKey) {
  if (geometryIndex_) {
    geometryIndex_->beginBuild();
  }
  Size rootAssigned{};
  bool const hasRootWidth = std::isfinite(constraints.maxWidth);
  bool const hasRootHeight = std::isfinite(constraints.maxHeight);
  if (hasRootWidth) {
    rootAssigned.width = std::max(0.f, constraints.maxWidth);
  }
  if (hasRootHeight) {
    rootAssigned.height = std::max(0.f, constraints.maxHeight);
  }
  pushFrame(constraints, LayoutHints{}, Point{}, std::move(rootKey), rootAssigned, hasRootWidth, hasRootHeight);
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

  auto retainExisting = [&](std::unique_ptr<SceneNode> node) -> std::unique_ptr<SceneNode> {
    StateStore* const store = StateStore::current();
    if (!node) {
      return nullptr;
    }
    node->position = node->retainedBuildStamp().localPosition;
    if (store) {
      store->markRetainedSubtreeVisited(frame().key);
    }
    if (geometryIndex_) {
      Point delta{};
      if (!frame().key.empty()) {
        if (std::optional<Rect> previousRect = geometryIndex_->forKey(frame().key)) {
          delta = Point{frame().origin.x - previousRect->x, frame().origin.y - previousRect->y};
        }
      }
      geometryIndex_->retainSubtree(frame().key, delta);
    }
    return node;
  };

  StateStore* const store = StateStore::current();
  if (!el.isComposite() && existing && store && store->currentCompositePathStable() &&
      canRetainExistingSubtree(el, el, *existing)) {
    return retainExisting(std::move(existing));
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
      if (store && existing && resolution.descendantsStable &&
          canRetainExistingSubtree(el, *resolution.body, *existing)) {
        return retainExisting(std::move(existing));
      }
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
  detail::ReusableSceneNodes reusable = detail::collectReusableSceneNodes(std::move(existingChildren));

  std::vector<std::unique_ptr<SceneNode>> next{};
  next.reserve(newChildren.size());
  for (std::size_t i = 0; i < newChildren.size(); ++i) {
    Element const& child = newChildren[i];
    NodeId const childId = SceneTree::childId(parent.id(), childLocalId(child, i));
    std::unique_ptr<SceneNode> reuse = detail::takeReusableNode(reusable, childId);
    next.push_back(buildOrReuse(child, childId, std::move(reuse)));
  }
  parent.replaceChildren(std::move(next));
}

std::unique_ptr<SceneNode> SceneBuilder::buildResolved(Element const& el, Element const& sceneEl, NodeId id,
                                                       std::unique_ptr<SceneNode> existing) {
  FrameState const current = frame();
  bool const resolvedFromComposite = (&sceneEl != &el);
  // `measureElement(el, ...)` may resolve and replace the same cached composite body again, so
  // keep a local copy of the resolved body while this call runs. Element copies now preserve
  // measure ids, so this no longer breaks subtree identity.
  Element stableSceneEl = sceneEl;

  detail::ElementModifiers const* mods = stableSceneEl.modifiers();
  detail::ElementModifiers const* outerMods = resolvedFromComposite ? el.modifiers() : nullptr;
  Point const subtreeOffset = modifierOffset(mods);
  Size const outerSize = measureElement(el, current.constraints, current.hints, current.key);
  Size layoutOuterSize = outerSize;
  if (stableSceneEl.typeTag() == ElementType::Text && textUsesContentBox(mods)) {
    layoutOuterSize = assignedOuterSizeForFrame(outerSize, current.constraints, current.assignedSize,
                                                current.hasAssignedWidth, current.hasAssignedHeight, mods);
  }
  EdgeInsets const padding = mods ? mods->padding : EdgeInsets{};
  LayoutConstraints innerConstraints = insetConstraints(current.constraints, padding);
  Size const paddedContentSize{
      std::max(0.f, layoutOuterSize.width - std::max(0.f, padding.left) - std::max(0.f, padding.right)),
      std::max(0.f, layoutOuterSize.height - std::max(0.f, padding.top) - std::max(0.f, padding.bottom)),
  };
  Size contentAssignedSize = current.assignedSize;
  if (current.hasAssignedWidth) {
    contentAssignedSize.width = std::max(0.f, current.assignedSize.width - std::max(0.f, padding.left) -
                                                  std::max(0.f, padding.right));
  }
  if (current.hasAssignedHeight) {
    contentAssignedSize.height = std::max(0.f, current.assignedSize.height - std::max(0.f, padding.top) -
                                                   std::max(0.f, padding.bottom));
  }
  LayoutConstraints contentBoxConstraints = innerConstraints;
  contentBoxConstraints.maxWidth = paddedContentSize.width;
  contentBoxConstraints.maxHeight = paddedContentSize.height;
  contentBoxConstraints.minWidth = std::min(contentBoxConstraints.minWidth, contentBoxConstraints.maxWidth);
  contentBoxConstraints.minHeight = std::min(contentBoxConstraints.minHeight, contentBoxConstraints.maxHeight);
  clampLayoutMinToMax(contentBoxConstraints);
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

  std::unique_ptr<SceneNode> innerExisting = std::move(existing);
  std::unique_ptr<ModifierSceneNode> outerModifierWrapper{};
  std::unique_ptr<SceneNode> outerLayoutWrapper{};
  std::unique_ptr<SceneNode> outerOverlay{};
  if (outerMods) {
    if (needsModifierWrapper(outerMods, false)) {
      outerModifierWrapper = takeTransparentModifierWrapper(innerExisting);
    }
    if (hasPadding(outerMods) || hasOverlay(outerMods)) {
      LayoutWrapperReuse reuse = takeTransparentLayoutWrapper(innerExisting);
      outerLayoutWrapper = std::move(reuse.wrapper);
      outerOverlay = std::move(reuse.overlay);
    }
  }

  if (stableSceneEl.typeTag() == ElementType::Unknown && stableSceneEl.isComposite()) {
    ComponentKey nestedKey = directCompositeBodyKey(current.key);
    pushFrame(current.constraints, current.hints, current.origin, std::move(nestedKey), current.assignedSize,
              current.hasAssignedWidth, current.hasAssignedHeight);
    std::unique_ptr<SceneNode> root = buildOrReuse(stableSceneEl, id, std::move(innerExisting));
    popFrame();

    std::unique_ptr<InteractionData> outerInteraction =
        outerMods ? makeInteractionData(outerMods, current.key) : nullptr;
    if (outerMods &&
        needsDecorationPass(outerMods, outerSize, rectSize(root->bounds), false, outerInteraction.get())) {
      root = decorateNode(std::move(root), el, outerMods, std::move(outerModifierWrapper),
                          std::move(outerLayoutWrapper), std::move(outerOverlay), outerSize, outerSize,
                          modifierOffset(outerMods), outerMods->padding, std::move(outerInteraction));
    }
    if (root->bounds.width <= 0.f && root->bounds.height <= 0.f) {
      root->bounds = sizeRect(outerSize);
    }
    stampRetainedBuild(*root, el, stableSceneEl);
    recordGeometry(rectSize(root->bounds));
    return root;
  }

  std::unique_ptr<ModifierSceneNode> modifierWrapper = takeTransparentModifierWrapper(innerExisting);
  LayoutWrapperReuse layoutReuse = takeTransparentLayoutWrapper(innerExisting);
  std::unique_ptr<SceneNode> layoutWrapper = std::move(layoutReuse.wrapper);
  std::unique_ptr<SceneNode> overlayNode = std::move(layoutReuse.overlay);

  std::unique_ptr<SceneNode> core{};
  std::unique_ptr<InteractionData> resolvedInteraction{};
  Size geometrySize = outerSize;
  switch (stableSceneEl.typeTag()) {
  case ElementType::Rectangle: {
    std::unique_ptr<RectSceneNode> rectNode = releaseAs<RectSceneNode>(std::move(innerExisting));
    if (!rectNode) {
      rectNode = std::make_unique<RectSceneNode>(id);
    }
    Theme const& theme = activeTheme(environment_);
    Size const resolvedRectSize =
        rectSize(assignedFrameForLeaf(paddedContentSize, innerConstraints, contentAssignedSize,
                                     current.hasAssignedWidth, current.hasAssignedHeight, mods, current.hints));
    bool dirty = false;
    dirty |= updateIfChanged(rectNode->size, resolvedRectSize);
    dirty |= updateIfChanged(rectNode->cornerRadius, mods ? mods->cornerRadius : CornerRadius{});
    dirty |= updateIfChanged(rectNode->fill, mods ? resolveFillStyle(mods->fill, theme) : FillStyle::none());
    dirty |= updateIfChanged(rectNode->stroke, mods ? resolveStrokeStyle(mods->stroke, theme) : StrokeStyle::none());
    dirty |= updateIfChanged(rectNode->shadow, mods ? resolveShadowStyle(mods->shadow, theme) : ShadowStyle::none());
    if (dirty) {
      rectNode->invalidatePaint();
      rectNode->markBoundsDirty();
    }
    rectNode->position = {};
    rectNode->recomputeBounds();
    geometrySize = resolvedRectSize;
    core = std::move(rectNode);
    break;
  }
  case ElementType::Text: {
    Text const& text = stableSceneEl.as<Text>();
    Theme const& theme = activeTheme(environment_);
    Font const resolvedFont = resolveFont(text.font, theme.bodyFont, theme);
    Color const resolvedColor = resolveColor(text.color, theme.labelColor, theme);
    Color const resolvedSelectionColor = resolveColor(text.selectionColor, theme.selectedContentBackgroundColor, theme);
    LayoutConstraints const& textFrameConstraints = textUsesContentBox(mods) ? contentBoxConstraints : innerConstraints;
    Rect const frameRect =
        assignedFrameForLeaf(paddedContentSize, textFrameConstraints, contentAssignedSize,
                             current.hasAssignedWidth, current.hasAssignedHeight, mods, current.hints);
    TextLayoutOptions const options = text_detail::makeTextLayoutOptions(text);
    std::string const displayText =
        text.selectable ? text.text
                        : text_detail::ellipsizedPlainText(text.text, resolvedFont, resolvedColor, frameRect,
                                                            options, textSystem_);
    std::shared_ptr<TextLayout const> textLayout = textSystem_.layout(displayText, resolvedFont, resolvedColor,
                                                                      Rect{0.f, 0.f, frameRect.width, frameRect.height},
                                                                      options);
    std::shared_ptr<detail::SelectableTextState> selectableState{};

    if (text.selectable && textLayout && text_detail::hasRenderableTextGeometry(*textLayout)) {
      selectableState = detail::selectableTextState(current.key);
      detail::updateSelectableTextLayout(*selectableState, textLayout, text.text, frameRect.width);

      std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(innerExisting));
      if (!group) {
        group = std::make_unique<SceneNode>(id);
      }
      detail::ReusableSceneNodes reusable = detail::releaseReusableChildren(*group);

      std::vector<std::unique_ptr<SceneNode>> nextChildren{};
      if (selectableState->selection.hasSelection()) {
        std::vector<Rect> const selectionRects =
            detail::selectionRects(selectableState->layoutResult, selectableState->selection,
                                   &selectableState->text, 0.f, 0.f);
        nextChildren.reserve(selectionRects.size() + 1);
        for (std::size_t i = 0; i < selectionRects.size(); ++i) {
          NodeId const rectId = SceneTree::childId(id, LocalId::fromIndex(i));
          std::unique_ptr<RectSceneNode> rectNode = detail::takeReusableNodeAs<RectSceneNode>(reusable, rectId);
          if (!rectNode) {
            rectNode = std::make_unique<RectSceneNode>(rectId);
          }
          Rect const rect = selectionRects[i];
          bool dirty = false;
          dirty |= updateIfChanged(rectNode->size, Size{rect.width, rect.height});
          dirty |= updateIfChanged(rectNode->fill, FillStyle::solid(resolvedSelectionColor));
          dirty |= updateIfChanged(rectNode->stroke, StrokeStyle::none());
          dirty |= updateIfChanged(rectNode->shadow, ShadowStyle::none());
          dirty |= updateIfChanged(rectNode->cornerRadius, CornerRadius{});
          if (dirty) {
            rectNode->invalidatePaint();
            rectNode->markBoundsDirty();
          }
          rectNode->position = Point{rect.x, rect.y};
          rectNode->recomputeBounds();
          nextChildren.push_back(std::move(rectNode));
        }
      }

      NodeId const textId = SceneTree::childId(id, LocalId::fromString("$text"));
      std::unique_ptr<TextSceneNode> textNode = detail::takeReusableNodeAs<TextSceneNode>(reusable, textId);
      if (!textNode) {
        textNode = std::make_unique<TextSceneNode>(textId);
      }
      configureTextSceneNode(*textNode, textSystem_, text, resolvedFont, resolvedColor, frameRect, displayText,
                             textLayout);
      nextChildren.push_back(std::move(textNode));

      group->replaceChildren(std::move(nextChildren));
      setGroupBounds(*group, rectSize(frameRect));
      resolvedInteraction = makeSelectableTextInteraction(mods, current.key, selectableState);
      core = std::move(group);
    } else {
      std::unique_ptr<TextSceneNode> textNode = releaseAs<TextSceneNode>(std::move(innerExisting));
      if (!textNode) {
        textNode = std::make_unique<TextSceneNode>(id);
      }
      configureTextSceneNode(*textNode, textSystem_, text, resolvedFont, resolvedColor, frameRect, displayText,
                             textLayout);
      core = std::move(textNode);
    }
    geometrySize = layoutOuterSize;
    break;
  }
  case ElementType::Render: {
    Render const& renderView = stableSceneEl.as<Render>();
    std::unique_ptr<RenderSceneNode> renderNode = releaseAs<RenderSceneNode>(std::move(innerExisting));
    if (!renderNode) {
      renderNode = std::make_unique<RenderSceneNode>(id);
    }
    Rect const frameRect =
        assignedFrameForLeaf(paddedContentSize, innerConstraints, contentAssignedSize,
                             current.hasAssignedWidth, current.hasAssignedHeight, mods, current.hints);
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
      renderNode->invalidatePaint();
      renderNode->markBoundsDirty();
    }
    renderNode->position = {};
    renderNode->recomputeBounds();
    geometrySize = layoutOuterSize;
    core = std::move(renderNode);
    break;
  }
  case ElementType::Image: {
    views::Image const& image = stableSceneEl.as<views::Image>();
    std::unique_ptr<ImageSceneNode> imageNode = releaseAs<ImageSceneNode>(std::move(innerExisting));
    if (!imageNode) {
      imageNode = std::make_unique<ImageSceneNode>(id);
    }
    Rect const frameRect =
        assignedFrameForLeaf(paddedContentSize, innerConstraints, contentAssignedSize,
                             current.hasAssignedWidth, current.hasAssignedHeight, mods, current.hints);
    bool dirty = false;
    dirty |= updateIfChanged(imageNode->image, image.source);
    dirty |= updateIfChanged(imageNode->size, Size{frameRect.width, frameRect.height});
    dirty |= updateIfChanged(imageNode->fillMode, image.fillMode);
    dirty |= updateIfChanged(imageNode->cornerRadius, mods ? mods->cornerRadius : CornerRadius{});
    dirty |= updateIfChanged(imageNode->opacity, mods ? mods->opacity : 1.f);
    if (dirty) {
      imageNode->invalidatePaint();
      imageNode->markBoundsDirty();
    }
    imageNode->position = {};
    imageNode->recomputeBounds();
    geometrySize = layoutOuterSize;
    core = std::move(imageNode);
    break;
  }
  case ElementType::Path: {
    PathShape const& path = stableSceneEl.as<PathShape>();
    std::unique_ptr<PathSceneNode> pathNode = releaseAs<PathSceneNode>(std::move(innerExisting));
    if (!pathNode) {
      pathNode = std::make_unique<PathSceneNode>(id);
    }
    Theme const& theme = activeTheme(environment_);
    bool dirty = false;
    dirty |= updateIfChanged(pathNode->path, path.path);
    dirty |= updateIfChanged(pathNode->fill, mods ? resolveFillStyle(mods->fill, theme) : FillStyle::none());
    dirty |= updateIfChanged(pathNode->stroke, mods ? resolveStrokeStyle(mods->stroke, theme) : StrokeStyle::none());
    dirty |= updateIfChanged(pathNode->shadow, mods ? resolveShadowStyle(mods->shadow, theme) : ShadowStyle::none());
    if (dirty) {
      pathNode->invalidatePaint();
      pathNode->markBoundsDirty();
    }
    pathNode->position = {};
    pathNode->recomputeBounds();
    core = std::move(pathNode);
    break;
  }
  case ElementType::Line: {
    Line const& line = stableSceneEl.as<Line>();
    std::unique_ptr<LineSceneNode> lineNode = releaseAs<LineSceneNode>(std::move(innerExisting));
    if (!lineNode) {
      lineNode = std::make_unique<LineSceneNode>(id);
    }
    Theme const& theme = activeTheme(environment_);
    bool dirty = false;
    dirty |= updateIfChanged(lineNode->from, line.from);
    dirty |= updateIfChanged(lineNode->to, line.to);
    dirty |= updateIfChanged(lineNode->stroke, resolveStrokeStyle(line.stroke, theme));
    if (dirty) {
      lineNode->invalidatePaint();
      lineNode->markBoundsDirty();
    }
    lineNode->position = {};
    lineNode->recomputeBounds();
    core = std::move(lineNode);
    break;
  }
  case ElementType::VStack: {
    VStack const& stack = stableSceneEl.as<VStack>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(innerExisting));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    detail::ReusableSceneNodes reusable = detail::releaseReusableChildren(*group);

    bool const widthAssigned = current.hasAssignedWidth && zStackAxisStretches(current.hints.zStackHorizontalAlign);
    bool const heightAssigned = current.hasAssignedHeight && zStackAxisStretches(current.hints.zStackVerticalAlign);
    float const availableW =
        std::isfinite(innerConstraints.maxWidth) ? std::max(0.f, innerConstraints.maxWidth) : 0.f;
    float const assignedW = widthAssigned ? std::max(0.f, contentAssignedSize.width) : 0.f;
    LayoutConstraints childConstraints = innerConstraints;
    childConstraints.maxHeight = std::numeric_limits<float>::infinity();
    childConstraints.maxWidth = availableW > 0.f ? availableW : std::numeric_limits<float>::infinity();
    clampLayoutMinToMax(childConstraints);
    LayoutHints childHints{};
    childHints.vStackCrossAlign = stack.alignment;

    std::vector<Size> sizes{};
    sizes.reserve(stack.children.size());
    std::vector<layout::StackMainAxisChild> stackChildren{};
    stackChildren.reserve(stack.children.size());
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      Element const& child = stack.children[i];
      ComponentKey childKey = current.key;
      childKey.push_back(childLocalId(child, i));
      Size const size = measureElement(child, childConstraints, childHints, childKey);
      sizes.push_back(size);
      stackChildren.push_back(layout::StackMainAxisChild{
          .naturalMainSize = size.height,
          .flexBasis = child.flexBasis(),
          .minMainSize = child.minMainSize(),
          .flexGrow = child.flexGrow(),
          .flexShrink = child.flexShrink(),
      });
    }
    bool const heightConstrained = heightAssigned;
    if (!heightConstrained) {
      warnFlexGrowIfParentMainAxisUnconstrained(stack.children, heightConstrained);
    }
    layout::StackMainAxisLayout const mainLayout =
        layout::layoutStackMainAxis(stackChildren, stack.spacing,
                                    heightAssigned ? std::max(0.f, contentAssignedSize.height) : 0.f,
                                    heightConstrained,
                                    stack.justifyContent);
    layout::StackLayoutResult const stackLayout =
        layout::layoutStack(layout::StackAxis::Vertical, stack.alignment, sizes, mainLayout.mainSizes,
                            mainLayout.itemSpacing, mainLayout.containerMainSize, mainLayout.startOffset,
                            assignedW, widthAssigned);

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(stack.children.size());
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      Element const& child = stack.children[i];
      LocalId const local = childLocalId(child, i);
      NodeId const childId = SceneTree::childId(id, local);
      std::unique_ptr<SceneNode> reuse = detail::takeReusableNode(reusable, childId);
      layout::StackSlot const& slot = stackLayout.slots[i];
      LayoutConstraints childBuild = innerConstraints;
      childBuild.maxWidth = slot.assignedSize.width > 0.f ? slot.assignedSize.width
                                                          : std::numeric_limits<float>::infinity();
      childBuild.maxHeight = slot.assignedSize.height;
      childBuild.minHeight = child.minMainSize();
      clampLayoutMinToMax(childBuild);
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      pushFrame(childBuild, childHints,
                Point{contentOrigin.x + slot.origin.x, contentOrigin.y + slot.origin.y},
                std::move(childKey), slot.assignedSize, slot.assignedSize.width > 0.f, true);
      std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
      popFrame();
      childNode->position.x += slot.origin.x;
      childNode->position.y += slot.origin.y;
      nextChildren.push_back(std::move(childNode));
    }
    group->replaceChildren(std::move(nextChildren));
    setAssignedGroupBounds(*group, stackLayout.containerSize);
    core = std::move(group);
    break;
  }
  case ElementType::HStack: {
    HStack const& stack = stableSceneEl.as<HStack>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(innerExisting));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    detail::ReusableSceneNodes reusable = detail::releaseReusableChildren(*group);

    bool const widthAssigned = current.hasAssignedWidth && zStackAxisStretches(current.hints.zStackHorizontalAlign);
    bool const heightAssigned = current.hasAssignedHeight && zStackAxisStretches(current.hints.zStackVerticalAlign);
    float const assignedW =
        widthAssigned ? resolvedAssignedSpan(contentAssignedSize.width, current.hasAssignedWidth, innerConstraints.maxWidth)
                      : 0.f;
    float const assignedH = heightAssigned ? std::max(0.f, contentAssignedSize.height) : 0.f;
    bool const heightConstrained = heightAssigned;
    bool const widthConstrained =
        widthAssigned ||
        (zStackAxisStretches(current.hints.zStackHorizontalAlign) && stack.children.size() == 1 &&
         std::isfinite(innerConstraints.maxWidth) && innerConstraints.maxWidth > 0.f);
    LayoutConstraints childConstraints = innerConstraints;
    childConstraints.maxWidth = std::numeric_limits<float>::infinity();
    childConstraints.maxHeight =
        stack.alignment == Alignment::Stretch && heightConstrained ? assignedH : std::numeric_limits<float>::infinity();
    if (stack.children.size() == 1 && widthConstrained) {
      childConstraints.maxWidth = assignedW;
    }
    clampLayoutMinToMax(childConstraints);

    std::vector<Size> sizes{};
    sizes.reserve(stack.children.size());
    std::vector<layout::StackMainAxisChild> stackChildren{};
    stackChildren.reserve(stack.children.size());
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      Element const& child = stack.children[i];
      ComponentKey childKey = current.key;
      childKey.push_back(childLocalId(child, i));
      Size const size = measureElement(child, childConstraints, LayoutHints{}, childKey);
      sizes.push_back(size);
      stackChildren.push_back(layout::StackMainAxisChild{
          .naturalMainSize = size.width,
          .flexBasis = child.flexBasis(),
          .minMainSize = child.minMainSize(),
          .flexGrow = child.flexGrow(),
          .flexShrink = child.flexShrink(),
      });
    }

    if (!widthConstrained) {
      warnFlexGrowIfParentMainAxisUnconstrained(stack.children, widthConstrained);
    }
    layout::StackMainAxisLayout const mainLayout =
        layout::layoutStackMainAxis(stackChildren, stack.spacing, assignedW, widthConstrained,
                                    stack.justifyContent);

    float rowInnerH = 0.f;
    std::vector<Size> rowSizes{};
    rowSizes.reserve(stack.children.size());
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      LayoutConstraints childMeasure = innerConstraints;
      childMeasure.maxWidth = mainLayout.mainSizes[i];
      childMeasure.maxHeight =
          stack.alignment == Alignment::Stretch && heightConstrained ? assignedH : std::numeric_limits<float>::infinity();
      clampLayoutMinToMax(childMeasure);
      LayoutHints rowHints{};
      rowHints.hStackCrossAlign = stack.alignment;
      ComponentKey childKey = current.key;
      childKey.push_back(childLocalId(stack.children[i], i));
      Size const measured = measureElement(stack.children[i], childMeasure, rowHints, childKey);
      rowSizes.push_back(measured);
      rowInnerH = std::max(rowInnerH, measured.height);
    }
    float const rowCrossSize = heightConstrained ? assignedH : rowInnerH;
    layout::StackLayoutResult const stackLayout =
        layout::layoutStack(layout::StackAxis::Horizontal, stack.alignment, rowSizes, mainLayout.mainSizes,
                            mainLayout.itemSpacing, mainLayout.containerMainSize, mainLayout.startOffset,
                            rowCrossSize, heightConstrained);

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(stack.children.size());
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      Element const& child = stack.children[i];
      LocalId const local = childLocalId(child, i);
      NodeId const childId = SceneTree::childId(id, local);
      std::unique_ptr<SceneNode> reuse = detail::takeReusableNode(reusable, childId);
      layout::StackSlot const& slot = stackLayout.slots[i];
      LayoutConstraints childBuild = innerConstraints;
      childBuild.maxWidth = slot.assignedSize.width;
      childBuild.maxHeight = slot.assignedSize.height;
      childBuild.minWidth = child.minMainSize();
      clampLayoutMinToMax(childBuild);
      LayoutHints rowHints{};
      rowHints.hStackCrossAlign = stack.alignment;
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      pushFrame(childBuild, rowHints,
                Point{contentOrigin.x + slot.origin.x, contentOrigin.y + slot.origin.y},
                std::move(childKey), slot.assignedSize, true, slot.assignedSize.height > 0.f);
      std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
      popFrame();
      childNode->position.x += slot.origin.x;
      childNode->position.y += slot.origin.y;
      nextChildren.push_back(std::move(childNode));
    }
    group->replaceChildren(std::move(nextChildren));
    setAssignedGroupBounds(*group, stackLayout.containerSize);
    core = std::move(group);
    break;
  }
  case ElementType::ZStack: {
    ZStack const& stack = stableSceneEl.as<ZStack>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(innerExisting));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    detail::ReusableSceneNodes reusable = detail::releaseReusableChildren(*group);

    float innerW = resolvedAssignedSpan(contentAssignedSize.width, current.hasAssignedWidth, innerConstraints.maxWidth);
    float innerH =
        resolvedAssignedSpan(contentAssignedSize.height, current.hasAssignedHeight, innerConstraints.maxHeight);
    LayoutConstraints childConstraints = innerConstraints;
    childConstraints.maxWidth = innerW > 0.f ? innerW : std::numeric_limits<float>::infinity();
    childConstraints.maxHeight = innerH > 0.f ? innerH : std::numeric_limits<float>::infinity();
    clampLayoutMinToMax(childConstraints);
    LayoutHints childHints{};
    childHints.zStackHorizontalAlign = stack.horizontalAlignment;
    childHints.zStackVerticalAlign = stack.verticalAlignment;
    float maxW = 0.f;
    float maxH = 0.f;
    std::vector<Size> sizes{};
    sizes.reserve(stack.children.size());
    for (std::size_t i = 0; i < stack.children.size(); ++i) {
      Element const& child = stack.children[i];
      ComponentKey childKey = current.key;
      childKey.push_back(childLocalId(child, i));
      Size const size = measureElement(child, childConstraints, childHints, childKey);
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
      std::unique_ptr<SceneNode> reuse = detail::takeReusableNode(reusable, childId);
      LayoutConstraints childBuild{};
      childBuild.maxWidth = innerW;
      childBuild.maxHeight = innerH;
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      // ZStack owns the full shared slot. Its children should also see that full available space
      // so nested layouts can expand and resolve against the actual container size.
      pushFrame(childBuild, childHints, contentOrigin, std::move(childKey), Size{innerW, innerH},
                innerW > 0.f, innerH > 0.f);
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
    Grid const& grid = stableSceneEl.as<Grid>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(innerExisting));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    detail::ReusableSceneNodes reusable = detail::releaseReusableChildren(*group);

    float const innerW =
        resolvedAssignedSpan(contentAssignedSize.width, current.hasAssignedWidth, innerConstraints.maxWidth);
    float const innerH =
        resolvedAssignedSpan(contentAssignedSize.height, current.hasAssignedHeight, innerConstraints.maxHeight);
    std::size_t const n = grid.children.size();
    std::vector<std::size_t> spans(n, 1u);
    for (std::size_t i = 0; i < n && i < grid.columnSpans.size(); ++i) {
      spans[i] = grid.columnSpans[i];
    }
    layout::GridTrackMetrics const metrics =
        layout::resolveGridTrackMetrics(grid.columns, spans, grid.horizontalSpacing, grid.verticalSpacing,
                                        innerW, innerW > 0.f, innerH, innerH > 0.f);

    std::vector<Size> sizes{};
    sizes.reserve(n);
    for (std::size_t i = 0; i < grid.children.size(); ++i) {
      Element const& child = grid.children[i];
      ComponentKey childKey = current.key;
      childKey.push_back(childLocalId(child, i));
      LayoutConstraints const childConstraints = layout::gridChildConstraints(innerConstraints, metrics, i);
      sizes.push_back(measureElement(child, childConstraints, LayoutHints{}, childKey));
    }
    layout::GridLayoutResult const gridLayout =
        layout::layoutGrid(metrics, grid.horizontalSpacing, grid.verticalSpacing,
                           innerW, innerW > 0.f, innerH, innerH > 0.f, sizes);

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(n);
    for (std::size_t index = 0; index < n; ++index) {
      Element const& child = grid.children[index];
      LocalId const local = childLocalId(child, index);
      NodeId const childId = SceneTree::childId(id, local);
      std::unique_ptr<SceneNode> reuse = detail::takeReusableNode(reusable, childId);
      Rect const slot = gridLayout.slots[index];
      LayoutConstraints childBuild = layout::gridChildConstraints(innerConstraints, metrics, index);
      childBuild.maxWidth = slot.width;
      childBuild.maxHeight = slot.height;
      clampLayoutMinToMax(childBuild);
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      pushFrame(childBuild, LayoutHints{}, Point{contentOrigin.x + slot.x, contentOrigin.y + slot.y},
                std::move(childKey), Size{slot.width, slot.height}, true, true);
      std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
      popFrame();
      childNode->position.x += slot.x + hAlignOffset(childNode->bounds.width, slot.width, grid.horizontalAlignment);
      childNode->position.y += slot.y + vAlignOffset(childNode->bounds.height, slot.height, grid.verticalAlignment);
      nextChildren.push_back(std::move(childNode));
    }
    group->replaceChildren(std::move(nextChildren));
    setGroupBounds(*group, gridLayout.containerSize);
    core = std::move(group);
    break;
  }
  case ElementType::OffsetView: {
    OffsetView const& offsetView = stableSceneEl.as<OffsetView>();
    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(innerExisting));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    detail::ReusableSceneNodes reusable = detail::releaseReusableChildren(*group);

    Size const viewport{
        current.hasAssignedWidth ? std::max(0.f, contentAssignedSize.width)
                                 : std::max(0.f, assignedSpan(0.f, innerConstraints.maxWidth)),
        current.hasAssignedHeight ? std::max(0.f, contentAssignedSize.height)
                                  : std::max(0.f, assignedSpan(0.f, innerConstraints.maxHeight)),
    };
    if (offsetView.viewportSize.signal && !sizeApproximatelyEqual(*offsetView.viewportSize, viewport)) {
      offsetView.viewportSize.setSilently(viewport);
    }
    LayoutConstraints const childConstraints = layout::scrollChildConstraints(offsetView.axis, innerConstraints, viewport);
    std::vector<Size> sizes{};
    sizes.reserve(offsetView.children.size());
    for (std::size_t i = 0; i < offsetView.children.size(); ++i) {
      Element const& child = offsetView.children[i];
      ComponentKey childKey = current.key;
      childKey.push_back(childLocalId(child, i));
      sizes.push_back(measureElement(child, childConstraints, LayoutHints{}, childKey));
    }
    layout::ScrollContentLayout const scrollLayout =
        layout::layoutScrollContent(offsetView.axis, viewport, offsetView.offset, sizes);
    Size const contentSize = scrollLayout.contentSize;
    if (offsetView.contentSize.signal && !sizeApproximatelyEqual(*offsetView.contentSize, contentSize)) {
      offsetView.contentSize.setSilently(contentSize);
    }

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(offsetView.children.size());
    for (std::size_t i = 0; i < offsetView.children.size(); ++i) {
      Element const& child = offsetView.children[i];
      LocalId const local = childLocalId(child, i);
      NodeId const childId = SceneTree::childId(id, local);
      std::unique_ptr<SceneNode> reuse = detail::takeReusableNode(reusable, childId);
      layout::ScrollChildSlot const& slot = scrollLayout.slots[i];
      LayoutConstraints childBuild = childConstraints;
      if (slot.assignedSize.width > 0.f) {
        childBuild.maxWidth = slot.assignedSize.width;
      }
      if (slot.assignedSize.height > 0.f) {
        childBuild.maxHeight = slot.assignedSize.height;
      }
      clampLayoutMinToMax(childBuild);
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      pushFrame(childBuild, LayoutHints{},
                Point{contentOrigin.x + slot.origin.x, contentOrigin.y + slot.origin.y},
                std::move(childKey), slot.assignedSize, slot.assignedSize.width > 0.f,
                slot.assignedSize.height > 0.f);
      std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
      popFrame();
      childNode->position.x += slot.origin.x;
      childNode->position.y += slot.origin.y;
      nextChildren.push_back(std::move(childNode));
    }
    group->replaceChildren(std::move(nextChildren));
    setGroupBounds(*group, contentSize);
    core = std::move(group);
    break;
  }
  case ElementType::ScrollView: {
    ScrollView const& scrollView = stableSceneEl.as<ScrollView>();
    ComponentKey scrollStateKey = current.key;
    scrollStateKey.push_back(LocalId::fromString("$scroll-state"));
    StateStore* const store = StateStore::current();
    if (store) {
      store->pushComponent(scrollStateKey, std::type_index(typeid(ScrollView)));
    }
    struct ScrollComponentPop {
      StateStore* store = nullptr;
      ~ScrollComponentPop() {
        if (store) {
          store->popComponent();
        }
      }
    } scrollComponentPop{store};

    State<Point> offsetState = scrollView.scrollOffset;
    if (!offsetState.signal && store) {
      offsetState = State<Point>{&store->claimSlot<Signal<Point>>(Point{})};
    }
    State<Size> viewportState = scrollView.viewportSize;
    if (!viewportState.signal && store) {
      viewportState = State<Size>{&store->claimSlot<Signal<Size>>(Size{})};
    }
    State<Size> contentState = scrollView.contentSize;
    if (!contentState.signal && store) {
      contentState = State<Size>{&store->claimSlot<Signal<Size>>(Size{})};
    }
    Animation<float>* indicatorOpacityAnimation = nullptr;
    if (store) {
      indicatorOpacityAnimation = &store->claimSlot<Animation<float>>(0.f);
    }
    State<Point> downPointState{};
    State<bool> draggingState{};
    if (store) {
      downPointState = State<Point>{&store->claimSlot<Signal<Point>>(Point{})};
      draggingState = State<bool>{&store->claimSlot<Signal<bool>>(false)};
    }

    ScrollAxis const ax = scrollView.axis;
    Point scrollOffset = offsetState.signal ? *offsetState : Point{};
    Size const viewport = current.hasAssignedWidth || current.hasAssignedHeight ? contentAssignedSize : outerSize;
    if (viewportState.signal && !sizeApproximatelyEqual(*viewportState, viewport)) {
      viewportState.setSilently(viewport);
    }
    LayoutConstraints const childConstraints = layout::scrollChildConstraints(ax, innerConstraints, viewport);

    std::vector<Element> const& contentChildren = scrollView.children;
    std::vector<Size> sizes{};
    sizes.reserve(contentChildren.size());
    for (std::size_t i = 0; i < contentChildren.size(); ++i) {
      Element const& child = contentChildren[i];
      ComponentKey childKey = current.key;
      childKey.push_back(childLocalId(child, i));
      sizes.push_back(measureElement(child, childConstraints, LayoutHints{}, childKey));
    }
    layout::ScrollContentLayout const scrollLayout =
        layout::layoutScrollContent(ax, viewport, scrollOffset, sizes);
    Size const contentSize = scrollLayout.contentSize;
    if (contentState.signal && !sizeApproximatelyEqual(*contentState, contentSize)) {
      contentState.setSilently(contentSize);
    }
    scrollOffset = scrollLayout.clampedOffset;
    if (offsetState.signal && scrollOffset != *offsetState) {
      offsetState.setSilently(scrollOffset);
    }

    Point const scrollRange = layout::maxScrollOffset(ax, viewport, contentSize);
    bool const showsVerticalIndicator = scrollRange.y > 0.f;
    bool const showsHorizontalIndicator = scrollRange.x > 0.f;
    bool const showsAnyIndicator = showsVerticalIndicator || showsHorizontalIndicator;
    Theme const& theme = activeTheme(environment_);
    Color const indicatorColor = scrollIndicatorColorForTheme(theme);
    Transition const indicatorShow = Transition::instant();
    Transition const indicatorHide = Transition::linear(theme.durationMedium).delayed(0.85f);
    float const indicatorOpacity =
        indicatorOpacityAnimation ? indicatorOpacityAnimation->get() : 0.f;
    layout::ScrollIndicatorMetrics const verticalIndicator =
        layout::makeVerticalIndicator(scrollOffset, viewport, contentSize, showsHorizontalIndicator);
    layout::ScrollIndicatorMetrics const horizontalIndicator =
        layout::makeHorizontalIndicator(scrollOffset, viewport, contentSize, showsVerticalIndicator);

    std::unique_ptr<ModifierSceneNode> modifier = releaseAs<ModifierSceneNode>(std::move(innerExisting));
    if (!modifier) {
      modifier = std::make_unique<ModifierSceneNode>(id);
    }
    std::unique_ptr<SceneNode> existingViewportGroup{};
    if (!modifier->children().empty()) {
      std::vector<std::unique_ptr<SceneNode>> children = modifier->releaseChildren();
      if (!children.empty()) {
        existingViewportGroup = std::move(children.front());
      }
    }
    std::unique_ptr<SceneNode> viewportGroup = releasePlainGroup(std::move(existingViewportGroup));
    if (!viewportGroup) {
      viewportGroup = std::make_unique<SceneNode>(SceneTree::childId(id, LocalId::fromString("$content")));
    }
    detail::ReusableSceneNodes reusableViewport = detail::releaseReusableChildren(*viewportGroup);

    NodeId const scrolledGroupId = SceneTree::childId(viewportGroup->id(), LocalId::fromString("$scroll"));
    std::unique_ptr<SceneNode> existingScrolledGroup = detail::takeReusableNode(reusableViewport, scrolledGroupId);
    std::unique_ptr<SceneNode> scrolledGroup = releasePlainGroup(std::move(existingScrolledGroup));
    if (!scrolledGroup) {
      scrolledGroup = std::make_unique<SceneNode>(scrolledGroupId);
    }
    detail::ReusableSceneNodes reusable = detail::releaseReusableChildren(*scrolledGroup);

    std::vector<std::unique_ptr<SceneNode>> scrolledChildren{};
    scrolledChildren.reserve(contentChildren.size());
    for (std::size_t i = 0; i < contentChildren.size(); ++i) {
      Element const& child = contentChildren[i];
      LocalId const local = childLocalId(child, i);
      NodeId const childId = SceneTree::childId(scrolledGroup->id(), local);
      std::unique_ptr<SceneNode> reuse = detail::takeReusableNode(reusable, childId);
      layout::ScrollChildSlot const& slot = scrollLayout.slots[i];
      ComponentKey childKey = current.key;
      childKey.push_back(local);
      pushFrame(childConstraints, LayoutHints{},
                Point{contentOrigin.x + slot.origin.x, contentOrigin.y + slot.origin.y},
                std::move(childKey), slot.assignedSize, slot.assignedSize.width > 0.f,
                slot.assignedSize.height > 0.f);
      std::unique_ptr<SceneNode> childNode = buildOrReuse(child, childId, std::move(reuse));
      popFrame();
      childNode->position.x += slot.origin.x;
      childNode->position.y += slot.origin.y;
      scrolledChildren.push_back(std::move(childNode));
    }
    scrolledGroup->replaceChildren(std::move(scrolledChildren));
    setGroupBounds(*scrolledGroup, contentSize);

    auto updateIndicatorNode =
        [&](detail::ReusableSceneNodes& reusableMap, NodeId indicatorId, layout::ScrollIndicatorMetrics const& metrics,
            bool vertical) -> std::unique_ptr<SceneNode> {
      std::unique_ptr<RectSceneNode> rectNode = detail::takeReusableNodeAs<RectSceneNode>(reusableMap, indicatorId);
      if (!rectNode) {
        rectNode = std::make_unique<RectSceneNode>(indicatorId);
      }
      bool dirty = false;
      dirty |= updateIfChanged(rectNode->size, Size{metrics.width, metrics.height});
      dirty |= updateIfChanged(rectNode->cornerRadius,
                               CornerRadius{vertical ? metrics.width * 0.5f : metrics.height * 0.5f});
      dirty |= updateIfChanged(rectNode->fill, FillStyle::solid(indicatorColor));
      dirty |= updateIfChanged(rectNode->stroke, StrokeStyle::none());
      dirty |= updateIfChanged(rectNode->shadow, ShadowStyle::none());
      if (dirty) {
        rectNode->invalidatePaint();
        rectNode->markBoundsDirty();
      }
      rectNode->position = Point{metrics.x, metrics.y};
      rectNode->recomputeBounds();
      return rectNode;
    };

    std::vector<std::unique_ptr<SceneNode>> viewportNext{};
    viewportNext.reserve(2);
    viewportNext.push_back(std::move(scrolledGroup));
    if (showsAnyIndicator) {
      NodeId const indicatorOverlayId =
          SceneTree::childId(viewportGroup->id(), LocalId::fromString("$indicators"));
      std::unique_ptr<ModifierSceneNode> indicatorOverlay =
          detail::takeReusableNodeAs<ModifierSceneNode>(reusableViewport, indicatorOverlayId);
      if (!indicatorOverlay) {
        indicatorOverlay = std::make_unique<ModifierSceneNode>(indicatorOverlayId);
      }
      indicatorOverlay->clip.reset();
      indicatorOverlay->opacity = indicatorOpacity;
      indicatorOverlay->blendMode = BlendMode::Normal;
      indicatorOverlay->fill = FillStyle::none();
      indicatorOverlay->stroke = StrokeStyle::none();
      indicatorOverlay->shadow = ShadowStyle::none();
      indicatorOverlay->cornerRadius = {};
      indicatorOverlay->position = {};

      std::vector<std::unique_ptr<SceneNode>> indicatorChildren{};
      indicatorChildren.reserve(2);
      if (verticalIndicator.visible()) {
        indicatorChildren.push_back(updateIndicatorNode(
            reusableViewport, SceneTree::childId(indicatorOverlay->id(), LocalId::fromString("$v-indicator")),
            verticalIndicator, true));
      }
      if (horizontalIndicator.visible()) {
        indicatorChildren.push_back(updateIndicatorNode(
            reusableViewport, SceneTree::childId(indicatorOverlay->id(), LocalId::fromString("$h-indicator")),
            horizontalIndicator, false));
      }
      indicatorOverlay->replaceChildren(std::move(indicatorChildren));
      indicatorOverlay->recomputeBounds();
      viewportNext.push_back(std::move(indicatorOverlay));
    }
    viewportGroup->replaceChildren(std::move(viewportNext));
    setGroupBounds(*viewportGroup, Size{std::max(contentSize.width, viewport.width),
                                        std::max(contentSize.height, viewport.height)});

    auto interaction = makeInteractionData(mods, current.key);
    if (!interaction) {
      interaction = std::make_unique<InteractionData>();
    }
    interaction->stableTargetKey = current.key;
    std::function<void(Point)> priorPointerDown = interaction->onPointerDown;
    std::function<void(Point)> priorPointerMove = interaction->onPointerMove;
    std::function<void(Point)> priorPointerUp = interaction->onPointerUp;
    std::function<void(Vec2)> priorScroll = interaction->onScroll;

    bool const dragScroll = scrollView.dragScrollEnabled;
    auto revealIndicators = [indicatorOpacityAnimation, indicatorShow, indicatorHide, showsAnyIndicator]() {
      if (!indicatorOpacityAnimation || !showsAnyIndicator) {
        return;
      }
      indicatorOpacityAnimation->set(1.f, indicatorShow);
      indicatorOpacityAnimation->set(0.f, indicatorHide);
    };
    interaction->onPointerDown =
        [priorPointerDown, dragScroll, draggingState, downPointState, offsetState](Point p) {
          if (priorPointerDown) {
            priorPointerDown(p);
          }
          if (!dragScroll || !draggingState.signal || !downPointState.signal || !offsetState.signal) {
            return;
          }
          draggingState = true;
          downPointState = Point{p.x + (*offsetState).x, p.y + (*offsetState).y};
        };
    interaction->onPointerUp = [priorPointerUp, dragScroll, draggingState](Point p) {
      if (priorPointerUp) {
        priorPointerUp(p);
      }
      if (!dragScroll || !draggingState.signal) {
        return;
      }
      draggingState = false;
    };
    interaction->onPointerMove =
        [priorPointerMove, dragScroll, draggingState, downPointState, ax, contentState, viewport,
         offsetState, revealIndicators](Point p) {
          if (priorPointerMove) {
            priorPointerMove(p);
          }
          if (!dragScroll || !draggingState.signal || !downPointState.signal || !offsetState.signal ||
              !contentState.signal || !*draggingState) {
            return;
          }
          Point const next{(*downPointState).x - p.x, (*downPointState).y - p.y};
          offsetState = layout::clampScrollOffset(ax, next, viewport, *contentState);
          revealIndicators();
        };
    interaction->onScroll =
        [priorScroll, ax, offsetState, contentState, viewport, revealIndicators](Vec2 d) {
          if (priorScroll) {
            priorScroll(d);
          }
          if (!offsetState.signal || !contentState.signal) {
            return;
          }
          Point next = *offsetState;
          if (ax == ScrollAxis::Vertical || ax == ScrollAxis::Both) {
            next.y -= d.y;
          }
          if (ax == ScrollAxis::Horizontal || ax == ScrollAxis::Both) {
            next.x -= d.x;
          }
          offsetState = layout::clampScrollOffset(ax, next, viewport, *contentState);
          revealIndicators();
        };
    resolvedInteraction = std::move(interaction);

    modifier->replaceChildren({});
    modifier->appendChild(std::move(viewportGroup));
    modifier->clip = Rect{0.f, 0.f, viewport.width, viewport.height};
    modifier->opacity = 1.f;
    modifier->fill = FillStyle::none();
    modifier->stroke = StrokeStyle::none();
    modifier->shadow = ShadowStyle::none();
    modifier->cornerRadius = {};
    modifier->recomputeBounds();
    geometrySize = viewport;
    core = std::move(modifier);
    break;
  }
  case ElementType::ScaleAroundCenter: {
    ScaleAroundCenter const& scaled = stableSceneEl.as<ScaleAroundCenter>();
    std::unique_ptr<CustomTransformSceneNode> transformNode =
        releaseAs<CustomTransformSceneNode>(std::move(innerExisting));
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
    ComponentKey childKey = current.key;
    childKey.push_back(LocalId::fromString("$child"));
    childKey = current.key;
    childKey.push_back(LocalId::fromString("$child"));
    pushFrame(innerConstraints, LayoutHints{}, contentOrigin, childKey, contentAssignedSize,
              current.hasAssignedWidth, current.hasAssignedHeight);
    std::unique_ptr<SceneNode> childNode =
        buildOrReuse(scaled.child, SceneTree::childId(id, LocalId::fromString("$child")), std::move(existingChild));
    popFrame();
    childNode->position = {};
    transformNode->replaceChildren({});
    transformNode->appendChild(std::move(childNode));
    Rect const childBounds = transformNode->children().front()->bounds;
    Point const pivot{childBounds.width * 0.5f, childBounds.height * 0.5f};
    transformNode->transform =
        Mat3::translate(pivot) * Mat3::scale(scaled.scale) * Mat3::translate(Point{-pivot.x, -pivot.y});
    transformNode->recomputeBounds();
    core = std::move(transformNode);
    break;
  }
  case ElementType::PopoverCalloutShape: {
    PopoverCalloutShape const& callout = stableSceneEl.as<PopoverCalloutShape>();
    ComponentKey contentKey = current.key;
    contentKey.push_back(LocalId::fromString("$content"));
    LayoutConstraints const contentConstraints =
        layout::innerConstraintsForPopoverContent(callout, innerConstraints);
    Size const contentMeasured = measureElement(callout.content, contentConstraints, LayoutHints{}, contentKey);
    layout::PopoverCalloutLayout const calloutLayout =
        layout::layoutPopoverCallout(callout, contentMeasured, innerConstraints);

    std::unique_ptr<SceneNode> group = releasePlainGroup(std::move(innerExisting));
    if (!group) {
      group = std::make_unique<SceneNode>(id);
    }
    detail::ReusableSceneNodes reusable = detail::releaseReusableChildren(*group);

    NodeId const chromeId = SceneTree::childId(id, LocalId::fromString("$chrome"));
    std::unique_ptr<PathSceneNode> chromeNode = detail::takeReusableNodeAs<PathSceneNode>(reusable, chromeId);
    if (!chromeNode) {
      chromeNode = std::make_unique<PathSceneNode>(chromeId);
    }
    bool chromeDirty = false;
    chromeDirty |= updateIfChanged(chromeNode->path, calloutLayout.chromePath);
    chromeDirty |= updateIfChanged(chromeNode->fill, FillStyle::solid(callout.backgroundColor));
    chromeDirty |= updateIfChanged(chromeNode->stroke, StrokeStyle::solid(callout.borderColor, callout.borderWidth));
    chromeDirty |= updateIfChanged(chromeNode->shadow, ShadowStyle::none());
    if (chromeDirty) {
      chromeNode->invalidatePaint();
      chromeNode->markBoundsDirty();
    }
    chromeNode->position = {};
    chromeNode->recomputeBounds();

    NodeId const contentId = SceneTree::childId(id, LocalId::fromString("$content"));
    std::unique_ptr<SceneNode> reuseContent = detail::takeReusableNode(reusable, contentId);
    pushFrame(contentConstraints, LayoutHints{},
              Point{contentOrigin.x + calloutLayout.contentOrigin.x, contentOrigin.y + calloutLayout.contentOrigin.y},
              std::move(contentKey), contentMeasured, true, true);
    std::unique_ptr<SceneNode> contentNode =
        buildOrReuse(callout.content, contentId, std::move(reuseContent));
    popFrame();
    contentNode->position.x += calloutLayout.contentOrigin.x;
    contentNode->position.y += calloutLayout.contentOrigin.y;

    std::vector<std::unique_ptr<SceneNode>> nextChildren{};
    nextChildren.reserve(2);
    nextChildren.push_back(std::move(chromeNode));
    nextChildren.push_back(std::move(contentNode));
    group->replaceChildren(std::move(nextChildren));
    setGroupBounds(*group, calloutLayout.totalSize);
    geometrySize = calloutLayout.totalSize;
    core = std::move(group);
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

  std::unique_ptr<InteractionData> interaction = std::move(resolvedInteraction);
  if (!interaction) {
    interaction = makeInteractionData(mods, current.key);
  }
  std::unique_ptr<SceneNode> root =
      decorateNode(std::move(core), el, mods, std::move(modifierWrapper), std::move(layoutWrapper),
                   std::move(overlayNode), layoutOuterSize, outerSize, subtreeOffset, padding,
                   std::move(interaction));

  std::unique_ptr<InteractionData> outerInteraction =
      outerMods ? makeInteractionData(outerMods, current.key) : nullptr;
  if (outerMods && needsDecorationPass(outerMods, outerSize, rectSize(root->bounds), false, outerInteraction.get())) {
    root = decorateNode(std::move(root), el, outerMods, std::move(outerModifierWrapper),
                        std::move(outerLayoutWrapper), std::move(outerOverlay), outerSize, outerSize,
                        modifierOffset(outerMods), outerMods->padding, std::move(outerInteraction));
  }
  if (root->bounds.width <= 0.f && root->bounds.height <= 0.f) {
    root->bounds = sizeRect(outerSize);
  }
  stampRetainedBuild(*root, el, stableSceneEl);
  recordGeometry(geometrySize.width > 0.f || geometrySize.height > 0.f ? geometrySize : rectSize(root->bounds));
  return root;
}

} // namespace flux
