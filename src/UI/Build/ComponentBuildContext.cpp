#include "UI/Build/ComponentBuildContext.hpp"

#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/UI/SceneBuilder.hpp>

#include "UI/Build/ComponentBuildSupport.hpp"
#include "UI/Layout/LayoutHelpers.hpp"

namespace flux::detail {

ComponentBuildContext::ComponentBuildContext(SceneBuilder& builder, TraversalContext const& traversal,
                                             Element const& sourceElement, Element const& sceneElement,
                                             ElementType typeTag, ComponentKey sceneKey,
                                             ElementModifiers const* mods,
                                             bool hasOuterModifierLayers, LayoutConstraints innerConstraints,
                                             Point contentOrigin, Size contentAssignedSize)
    : builder_(builder)
    , traversal_(traversal)
    , sourceElement_(sourceElement)
    , sceneElement_(sceneElement)
    , typeTag_(typeTag)
    , sceneKey_(std::move(sceneKey))
    , mods_(mods)
    , hasOuterModifierLayers_(hasOuterModifierLayers)
    , innerConstraints_(innerConstraints)
    , contentOrigin_(contentOrigin)
    , contentAssignedSize_(contentAssignedSize) {}

TextSystem& ComponentBuildContext::textSystem() const {
  return builder_.textSystem_;
}

EnvironmentStack& ComponentBuildContext::environment() const {
  return builder_.environment_;
}

Theme const& ComponentBuildContext::theme() const {
  return build::activeTheme(builder_.environment_);
}

MeasureLayoutCache* ComponentBuildContext::measureLayoutCache() const noexcept {
  return builder_.measureLayoutCache_.get();
}

MeasureLayoutKey ComponentBuildContext::makeMeasureLayoutKey(LayoutConstraints const& constraints,
                                                             LayoutHints const& hints) const {
  return MeasureLayoutKey{
      .measureId = sceneElement_.measureId(),
      .componentKey = key(),
      .constraints = constraints,
      .hints = hints,
  };
}

ComponentKey ComponentBuildContext::childKey(LocalId local) const {
  ComponentKey const& currentKey = key();
  ComponentKey childKeyValue = currentKey;
  childKeyValue.push_back(local);
  return childKeyValue;
}

Size ComponentBuildContext::measureElement(Element const& element, ComponentKey const& key,
                                           LayoutConstraints const& constraints,
                                           LayoutHints const& hints) const {
  return builder_.measureElement(element, constraints, hints, key);
}

Size ComponentBuildContext::measureChild(Element const& element, LocalId local,
                                         LayoutConstraints const& constraints,
                                         LayoutHints const& hints) const {
  return measureElement(element, childKey(local), constraints, hints);
}

void ComponentBuildContext::recordChildMeasure(Element const& child, ComponentKey const& childKeyValue,
                                               LayoutConstraints const& constraints,
                                               LayoutHints const& hints, Size size) const {
  if (!builder_.measureLayoutCache_) {
    return;
  }
  builder_.measureLayoutCache_->recordElementSize(
      MeasureLayoutKey{
          .measureId = child.measureId(),
          .componentKey = childKeyValue,
          .constraints = constraints,
          .hints = hints,
      },
      size);
}

void ComponentBuildContext::recordMeasuredSize(Element const& child, LocalId local,
                                               LayoutConstraints const& constraints,
                                               LayoutHints const& hints, Size size) const {
  recordChildMeasure(child, childKey(local), constraints, hints, size);
}

std::unique_ptr<scenegraph::SceneNode>
ComponentBuildContext::buildChild(Element const& child, LocalId local, LayoutConstraints const& constraints,
                                  LayoutHints const& hints, Point origin, Size assignedSize,
                                  bool hasAssignedWidth, bool hasAssignedHeight,
                                  std::unique_ptr<scenegraph::SceneNode> existing) const {
  (void)existing;
  ComponentKey childKeyValue = childKey(local);
  builder_.pushFrame(constraints, hints, origin, std::move(childKeyValue), assignedSize, hasAssignedWidth,
                     hasAssignedHeight);
  std::unique_ptr<scenegraph::SceneNode> childNode = builder_.buildOrReuse(child, nullptr);
  builder_.popFrame();
  return childNode;
}

Size ComponentBuildContext::outerSize() const {
  if (!outerSize_) {
    outerSize_ = builder_.measureElement(sourceElement_, constraints(), hints(), key());
  }
  return *outerSize_;
}

Size ComponentBuildContext::layoutOuterSize() const {
  if (!layoutOuterSize_) {
    Size size = outerSize();
    if (typeTag_ == ElementType::Text && build::textUsesContentBox(mods_)) {
      size = build::assignedOuterSizeForFrame(size, constraints(), assignedSize(), hasAssignedWidth(),
                                              hasAssignedHeight(), mods_);
    }
    layoutOuterSize_ = size;
  }
  return *layoutOuterSize_;
}

Size ComponentBuildContext::paddedContentSize() const {
  if (!paddedContentSize_) {
    EdgeInsets const padding = mods_ ? mods_->padding : EdgeInsets{};
    Size const outer = layoutOuterSize();
    paddedContentSize_ = Size{
        std::max(0.f, outer.width - std::max(0.f, padding.left) - std::max(0.f, padding.right)),
        std::max(0.f, outer.height - std::max(0.f, padding.top) - std::max(0.f, padding.bottom)),
    };
  }
  return *paddedContentSize_;
}

LayoutConstraints ComponentBuildContext::contentBoxConstraints() const {
  if (!contentBoxConstraints_) {
    LayoutConstraints contentConstraints = innerConstraints_;
    Size const padded = paddedContentSize();
    contentConstraints.maxWidth = padded.width;
    contentConstraints.maxHeight = padded.height;
    contentConstraints.minWidth = std::min(contentConstraints.minWidth, contentConstraints.maxWidth);
    contentConstraints.minHeight = std::min(contentConstraints.minHeight, contentConstraints.maxHeight);
    layout::clampLayoutMinToMax(contentConstraints);
    contentBoxConstraints_ = contentConstraints;
  }
  return *contentBoxConstraints_;
}

void ComponentBuildContext::finalizeOuterSizes(Size contentSize, Size& outerSizeValue,
                                               Size& layoutOuterSizeValue) const {
  if (typeTag_ == ElementType::Text && build::textUsesContentBox(mods_)) {
    outerSizeValue = contentSize;
    layoutOuterSizeValue = contentSize;
    return;
  }

  if (!outerSize_ && !layoutOuterSize_ &&
      build::canDeferOuterMeasurement(typeTag_, mods_, hasOuterModifierLayers_)) {
    outerSizeValue = build::measuredOuterSizeFromContent(contentSize, mods_);
    layoutOuterSizeValue = outerSizeValue;
    return;
  }

  layoutOuterSizeValue = layoutOuterSize();
  outerSizeValue = outerSize_.value_or(layoutOuterSizeValue);
}

std::unique_ptr<scenegraph::InteractionData> ComponentBuildContext::makeInteractionData() const {
  return build::makeInteractionData(mods_, key());
}

std::unique_ptr<scenegraph::InteractionData>
ComponentBuildContext::makeSelectableTextInteraction(std::shared_ptr<SelectableTextState> const& state) const {
  return build::makeSelectableTextInteraction(mods_, key(), state);
}

ComponentBuildResult
ComponentBuildContext::buildFallback(std::unique_ptr<scenegraph::SceneNode> existing) const {
  (void)existing;
  ComponentBuildResult result{};
  result.node = std::make_unique<scenegraph::GroupNode>(build::sizeRect(paddedContentSize()));
  result.geometrySize = build::rectSize(result.node->bounds());
  result.hasGeometrySize = true;
  return result;
}

ComponentBuildResult buildMeasuredFallback(ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing) {
  return ctx.buildFallback(std::move(existing));
}

} // namespace flux::detail
