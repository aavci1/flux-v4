#pragma once

/// \file Flux/UI/Detail/ElementModifiers.hpp
///
/// Internal retained-UI state used by `Element` measurement/build plumbing.

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Detail/SmallVector.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

namespace flux {

class Element;
struct Popover;
struct Spacer;
class StateStore;

namespace detail {

struct ElementDeleter {
  void operator()(Element* element) const noexcept;
};

using OwnedElementPtr = std::unique_ptr<Element, ElementDeleter>;

std::uint64_t nextElementMeasureId();

Popover* popoverOverlayStateIf(Element& el);

struct CompositeBodyResolution {
  Element* body = nullptr;
  std::unique_ptr<Element> ownedBody{};
  bool descendantsStable = false;
};

inline LocalId compositeBodyLocalId() {
  return LocalId::fromString("$flux.body");
}

template<typename C, typename BuildFn>
CompositeBodyResolution resolveCompositeBody(StateStore* store, ComponentKey const& key,
                                             LayoutConstraints const& constraints, C const& value,
                                             BuildFn&& buildFn);

template<typename C>
float flexGrowOf(C const& v) {
  if constexpr (requires { v.flexGrow; }) {
    return v.flexGrow;
  } else if constexpr (std::is_same_v<C, Spacer>) {
    return 1.f;
  }
  return 0.f;
}

template<typename C>
float flexShrinkOf(C const& v) {
  if constexpr (requires { v.flexShrink; }) {
    return v.flexShrink;
  }
  return 0.f;
}

template<typename C>
std::optional<float> flexBasisOf(C const& v) {
  if constexpr (requires { v.flexBasis; }) {
    return std::max(0.f, v.flexBasis);
  }
  return std::nullopt;
}

template<typename C>
float minMainSizeOf(C const& v) {
  if constexpr (requires { v.minMainSize; }) {
    return v.minMainSize;
  }
  if constexpr (requires { v.minSize; }) {
    return v.minSize;
  }
  if constexpr (requires { v.minLength; }) {
    return std::max(0.f, v.minLength);
  }
  return 0.f;
}

struct ElementModifiers {
  EdgeInsets padding{};
  FillStyle fill = FillStyle::none();
  StrokeStyle stroke = StrokeStyle::none();
  ShadowStyle shadow = ShadowStyle::none();
  CornerRadius cornerRadius{};
  float opacity = 1.f;
  Vec2 translation{};
  bool clip = false;
  float positionX = 0.f;
  float positionY = 0.f;
  float sizeWidth = 0.f;
  float sizeHeight = 0.f;
  std::unique_ptr<Element> overlay;

  std::function<void()> onTap;
  std::function<void(Point)> onPointerDown;
  std::function<void(Point)> onPointerUp;
  std::function<void(Point)> onPointerMove;
  std::function<void(Vec2)> onScroll;
  std::function<void(KeyCode, Modifiers)> onKeyDown;
  std::function<void(KeyCode, Modifiers)> onKeyUp;
  std::function<void(std::string const&)> onTextInput;
  bool focusable = false;
  Cursor cursor = Cursor::Inherit;

  bool hasInteraction() const noexcept {
    return static_cast<bool>(onTap) || static_cast<bool>(onPointerDown) || static_cast<bool>(onPointerUp) ||
           static_cast<bool>(onPointerMove) || static_cast<bool>(onScroll) || static_cast<bool>(onKeyDown) ||
           static_cast<bool>(onKeyUp) || static_cast<bool>(onTextInput) || focusable ||
           cursor != Cursor::Inherit;
  }

  bool needsModifierPass() const {
    return !padding.isZero() || !fill.isNone() || !stroke.isNone() || !shadow.isNone() ||
           !cornerRadius.isZero() || opacity < 1.f - 1e-6f || std::fabs(translation.x) > 1e-6f ||
           std::fabs(translation.y) > 1e-6f || clip || std::fabs(positionX) > 1e-6f ||
           std::fabs(positionY) > 1e-6f || hasInteraction() || sizeWidth > 0.f || sizeHeight > 0.f ||
           overlay != nullptr;
  }

  ElementModifiers() = default;
  ElementModifiers(ElementModifiers const& o);
  ElementModifiers& operator=(ElementModifiers const& o);
  ElementModifiers(ElementModifiers&&) noexcept = default;
  ElementModifiers& operator=(ElementModifiers&&) noexcept = default;
  ~ElementModifiers();
};

[[nodiscard]] bool elementModifiersStructurallyEqual(ElementModifiers const& lhs,
                                                     ElementModifiers const& rhs) noexcept;

struct ResolvedElement {
  Element const* sceneElement = nullptr;
  SmallVector<OwnedElementPtr, 2> ownedBodies{};
  SmallVector<EnvironmentLayer, 4> environmentLayers{};
  SmallVector<ElementModifiers, 4> modifierLayers{};
  SmallVector<ComponentKey, 4> bodyComponentKeys{};
  ComponentKey stableInteractionKey{};
  bool nestSceneUnderFirstBody = false;
  bool descendantsStable = false;
};

} // namespace detail
} // namespace flux
