#include <Flux/UI/SceneBuilder.hpp>

#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>

#include "UI/Build/ComponentBuildSupport.hpp"

namespace flux {

namespace {

namespace build_support = ::flux::detail::build;

constexpr std::int8_t kUnsetAlignmentStamp = -1;

bool modifierInteractionHandlersComparable(detail::ElementModifiers const& lhs,
                                           detail::ElementModifiers const& rhs) noexcept {
  return !lhs.onTap && !rhs.onTap &&
         !lhs.onPointerDown && !rhs.onPointerDown &&
         !lhs.onPointerUp && !rhs.onPointerUp &&
         !lhs.onPointerMove && !rhs.onPointerMove &&
         !lhs.onScroll && !rhs.onScroll &&
         !lhs.onKeyDown && !rhs.onKeyDown &&
         !lhs.onKeyUp && !rhs.onKeyUp &&
         !lhs.onTextInput && !rhs.onTextInput;
}

bool modifierLayersStructurallyEqual(std::span<detail::ElementModifiers const> lhs,
                                     std::span<detail::ElementModifiers const> rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    detail::ElementModifiers const& lhsMods = lhs[i];
    detail::ElementModifiers const& rhsMods = rhs[i];
    if (!modifierInteractionHandlersComparable(lhsMods, rhsMods)) {
      return false;
    }
    if (lhsMods.padding != rhsMods.padding || lhsMods.fill != rhsMods.fill ||
        lhsMods.stroke != rhsMods.stroke || lhsMods.shadow != rhsMods.shadow ||
        lhsMods.cornerRadius != rhsMods.cornerRadius || lhsMods.opacity != rhsMods.opacity ||
        lhsMods.translation != rhsMods.translation || lhsMods.clip != rhsMods.clip ||
        lhsMods.positionX != rhsMods.positionX || lhsMods.positionY != rhsMods.positionY ||
        lhsMods.sizeWidth != rhsMods.sizeWidth || lhsMods.sizeHeight != rhsMods.sizeHeight ||
        lhsMods.focusable != rhsMods.focusable || lhsMods.cursor != rhsMods.cursor) {
      return false;
    }
    if (static_cast<bool>(lhsMods.overlay) != static_cast<bool>(rhsMods.overlay)) {
      return false;
    }
    if (lhsMods.overlay && !lhsMods.overlay->structuralEquals(*rhsMods.overlay)) {
      return false;
    }
  }
  return true;
}

bool resolvedEnvironmentLayersComparable(std::span<EnvironmentLayer const> layers) {
  return std::all_of(layers.begin(), layers.end(), [](EnvironmentLayer const& layer) { return layer.empty(); });
}

struct ComparableResolvedInput {
  Element sceneElement;
  std::vector<detail::ElementModifiers> modifierLayers{};
  std::vector<ComponentKey> bodyComponentKeys{};
  Theme theme = Theme::light();
};

bool canUseComparableResolvedRetainedStamp(detail::ResolvedElement const& resolved) {
  return resolved.sceneElement &&
         resolved.sceneElement->structuralEquals(*resolved.sceneElement) &&
         resolvedEnvironmentLayersComparable(resolved.environmentLayers) &&
         modifierLayersStructurallyEqual(resolved.modifierLayers, resolved.modifierLayers);
}

bool comparableResolvedInputsEqual(detail::ResolvedElement const& resolved,
                                   ComparableResolvedInput const& comparable,
                                   Theme const& activeTheme) {
  return resolved.sceneElement &&
         comparable.theme == activeTheme &&
         resolved.sceneElement->structuralEquals(comparable.sceneElement) &&
         resolved.bodyComponentKeys == comparable.bodyComponentKeys &&
         modifierLayersStructurallyEqual(resolved.modifierLayers, comparable.modifierLayers);
}

std::int8_t encodeAlignmentStamp(std::optional<Alignment> alignment) {
  if (!alignment) {
    return kUnsetAlignmentStamp;
  }
  return static_cast<std::int8_t>(static_cast<int>(*alignment));
}

} // namespace

bool SceneBuilder::canRetainExistingSubtree(detail::ResolvedElement const& resolved,
                                            SceneNode const& existing) const {
  StateStore* const store = StateStore::current();
  if (!store || !resolved.sceneElement || store->hasDirtyDescendant(frame().key)) {
    return false;
  }

  RetainedBuildStamp const& stamp = existing.retainedBuildStamp();
  auto const& current = frame();
  bool const identityMatched = stamp.measureId != 0 && stamp.measureId == resolved.sceneElement->measureId();
  ComparableResolvedInput const* comparableInput =
      static_cast<ComparableResolvedInput const*>(stamp.comparableInput.get());
  if (!identityMatched &&
      (!comparableInput ||
       !comparableResolvedInputsEqual(resolved, *comparableInput, build_support::activeTheme(environment_)))) {
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

void SceneBuilder::stampRetainedBuild(SceneNode& node, detail::ResolvedElement const& resolved) const {
  auto const& current = frame();
  RetainedBuildStamp stamp{
      .measureId = resolved.sceneElement ? resolved.sceneElement->measureId() : 0,
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
  };
  if (canUseComparableResolvedRetainedStamp(resolved)) {
    stamp.comparableInput = std::unique_ptr<void, void (*)(void*)>(
        new ComparableResolvedInput{
            .sceneElement = *resolved.sceneElement,
            .modifierLayers = resolved.modifierLayers,
            .bodyComponentKeys = resolved.bodyComponentKeys,
            .theme = build_support::activeTheme(environment_),
        },
        [](void* p) { delete static_cast<ComparableResolvedInput*>(p); });
  }
  node.setRetainedBuildStamp(std::move(stamp));
}

} // namespace flux
