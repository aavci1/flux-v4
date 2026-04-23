#pragma once

#include <Flux/Reactive/Detail/DependencyTracker.hpp>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

template<typename C>
Element& StateStore::commitBody(ComponentKey const& key, C const& value,
                                LayoutConstraints const& constraints, std::unique_ptr<Element> body,
                                std::vector<Observable*> deps) {
  ComponentState& state = states_[key];
  bool const preserveReusableConstraints =
      state.valueSnapshot.value != nullptr &&
      state.valueSnapshot.type == std::type_index(typeid(C)) &&
      state.valueSnapshot.equals != nullptr &&
      state.valueSnapshot.equals(state.valueSnapshot.value.get(), &value);

  for (ComponentSubscription const& sub : state.subscriptions) {
    if (sub.observable) {
      sub.observable->unobserve(sub.handle);
    }
  }
  state.subscriptions.clear();

  std::sort(deps.begin(), deps.end());
  deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
  for (Observable* dep : deps) {
    if (!dep) {
      continue;
    }
    ObserverHandle const handle = dep->observeComposite(*this, key);
    state.subscriptions.push_back(ComponentSubscription{.observable = dep, .handle = handle});
  }

  state.valueSnapshot = makeValueSnapshot(value);
  state.environmentDependencies = std::move(state.pendingEnvironmentDependencies);
  state.pendingEnvironmentDependencies.clear();
  Element* raw = body.release();
  state.lastBody = std::unique_ptr<void, void (*)(void*)>(
      raw, [](void* p) { delete static_cast<Element*>(p); });
  state.lastBodyEpoch = buildEpoch_;
  state.lastBodyConstraints = constraints;
  if (!preserveReusableConstraints) {
    state.reusableConstraints.clear();
  }
  bool hasRecordedConstraints = false;
  for (LayoutConstraints const& recorded : state.reusableConstraints) {
    if (constraintsEqual(recorded, constraints)) {
      hasRecordedConstraints = true;
      break;
    }
  }
  if (!hasRecordedConstraints) {
    state.reusableConstraints.push_back(constraints);
  }
  return *raw;
}

namespace detail {

template<typename C, typename BuildFn>
CompositeBodyResolution resolveCompositeBody(StateStore* store, ComponentKey const& key,
                                             LayoutConstraints const& constraints, C const& value,
                                             BuildFn&& buildFn) {
  CompositeBodyResolution resolution{};
  if (!store) {
    resolution.ownedBody = std::make_unique<Element>(std::invoke(std::forward<BuildFn>(buildFn)));
    resolution.body = resolution.ownedBody.get();
    return resolution;
  }
  if (store->hasBodyForCurrentRebuild(key, constraints)) {
    resolution.body = store->cachedBody(key);
    return resolution;
  }
  if (store->canReuseBody(key, value, constraints)) {
    // Cached body reuse skips body(), so allow components to refresh leading retained inputs
    // such as callback slots before the subtree is kept alive.
    if constexpr (requires(C const& component) { component.updateRetainedInputs(); }) {
      value.updateRetainedInputs();
    }
    resolution.body = store->cachedBody(key);
    resolution.descendantsStable = true;
    return resolution;
  }

  detail::DependencyTracker tracker;
  detail::DependencyTracker::push(&tracker);
  std::unique_ptr<Element> body;
  try {
    body = std::make_unique<Element>(std::invoke(std::forward<BuildFn>(buildFn)));
  } catch (...) {
    detail::DependencyTracker::pop();
    throw;
  }
  detail::DependencyTracker::pop();

  resolution.body = &store->commitBody(key, value, constraints, std::move(body), std::move(tracker.deps));
  return resolution;
}

} // namespace detail

template<typename C>
struct Element::Model : Concept {
  C value;
  explicit Model(C c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    if constexpr (std::is_copy_constructible_v<C>) {
      return std::make_unique<Model<C>>(value);
    } else {
      assert(false && "Non-copyable component cannot be placed in a children list");
      std::abort();
    }
  }
  ElementType elementType() const noexcept override;
  std::type_index modelType() const noexcept override { return std::type_index(typeid(C)); }
  void const* rawValuePtr() const noexcept override { return &value; }
  bool valueEquals(Concept const& other) const noexcept override {
    if (other.modelType() != std::type_index(typeid(C))) {
      return false;
    }
    C const& rhs = *static_cast<C const*>(other.rawValuePtr());
    if constexpr (detail::equalityComparableV<C>) {
      return value == rhs;
    } else if constexpr (std::is_trivially_copyable_v<C>) {
      return std::memcmp(&value, &rhs, sizeof(C)) == 0;
    } else {
      return false;
    }
  }
  bool expandsBody() const noexcept override { return ExpandsBodyComponent<C>; }
  detail::CompositeBodyResolution resolveCompositeBody(ComponentKey const& key,
                                                       LayoutConstraints const& constraints,
                                                       detail::ElementModifiers const* modifiers) const override;
  detail::ComponentBuildResult buildMeasured(detail::ComponentBuildContext& ctx,
                                             std::unique_ptr<scenegraph::SceneNode> existing) const override {
    if constexpr (MeasuredComponent<C>) {
      return detail::buildMeasuredComponent(value, ctx, std::move(existing));
    } else {
      (void)ctx;
      (void)existing;
      return {};
    }
  }
  Size measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  std::optional<float> flexBasis() const override { return detail::flexBasisOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
};

template<typename C>
detail::CompositeBodyResolution Element::Model<C>::resolveCompositeBody(ComponentKey const& key,
                                                                        LayoutConstraints const& constraints,
                                                                        detail::ElementModifiers const* modifiers) const {
  if constexpr (!ExpandsBodyComponent<C>) {
    (void)key;
    (void)constraints;
    (void)modifiers;
    return {};
  } else {
    StateStore* store = StateStore::current();
    if (!store) {
      detail::CompositeBodyResolution resolution{};
      resolution.ownedBody = std::make_unique<Element>(value.body());
      resolution.body = resolution.ownedBody.get();
      return resolution;
    }

    store->pushComponent(key, std::type_index(typeid(C)));
    store->pushCompositeConstraints(constraints);
    if (modifiers) {
      store->pushCompositeElementModifiers(modifiers);
    }

    detail::CompositeBodyResolution resolution{};
    try {
      resolution =
          detail::resolveCompositeBody(store, key, constraints, value, [&] { return value.body(); });
    } catch (...) {
      if (modifiers) {
        store->popCompositeElementModifiers();
      }
      store->popCompositeConstraints();
      store->popComponent();
      throw;
    }

    if (modifiers) {
      store->popCompositeElementModifiers();
    }
    store->popCompositeConstraints();
    store->popComponent();
    store->recordBodyConstraints(key, constraints);
    return resolution;
  }
}

template<typename C>
ElementType Element::Model<C>::elementType() const noexcept {
  if constexpr (std::is_same_v<C, Rectangle>) {
    return ElementType::Rectangle;
  } else if constexpr (std::is_same_v<C, Text>) {
    return ElementType::Text;
  } else if constexpr (std::is_same_v<C, views::Image>) {
    return ElementType::Image;
  } else if constexpr (std::is_same_v<C, PathShape>) {
    return ElementType::Path;
  } else if constexpr (std::is_same_v<C, Render>) {
    return ElementType::Render;
  } else if constexpr (std::is_same_v<C, VStack>) {
    return ElementType::VStack;
  } else if constexpr (std::is_same_v<C, HStack>) {
    return ElementType::HStack;
  } else if constexpr (std::is_same_v<C, ZStack>) {
    return ElementType::ZStack;
  } else if constexpr (std::is_same_v<C, Grid>) {
    return ElementType::Grid;
  } else if constexpr (std::is_same_v<C, OffsetView>) {
    return ElementType::OffsetView;
  } else if constexpr (std::is_same_v<C, ScrollView>) {
    return ElementType::ScrollView;
  } else if constexpr (std::is_same_v<C, ScaleAroundCenter>) {
    return ElementType::ScaleAroundCenter;
  } else if constexpr (std::is_same_v<C, Spacer>) {
    return ElementType::Spacer;
  } else if constexpr (std::is_same_v<C, PopoverCalloutShape>) {
    return ElementType::PopoverCalloutShape;
  } else {
    return ElementType::Unknown;
  }
}

template<typename C>
Size Element::Model<C>::measure(MeasureContext& ctx, LayoutConstraints const& constraints,
                                LayoutHints const& hints, TextSystem& textSystem) const {
  if constexpr (MeasuredComponent<C>) {
    return value.measure(ctx, constraints, hints, textSystem);
  } else if constexpr (ExpandsBodyComponent<C>) {
    ComponentKey const key = ctx.nextCompositeKey();
    StateStore* store = StateStore::current();
    detail::CompositeBodyResolution resolution{};
    if (store) {
      store->pushComponent(key, std::type_index(typeid(C)));
      store->pushCompositeConstraints(constraints);
      try {
        resolution =
            detail::resolveCompositeBody(store, key, constraints, value, [&] { return value.body(); });
      } catch (...) {
        store->popCompositeConstraints();
        store->popComponent();
        throw;
      }
      store->popCompositeConstraints();
      store->popComponent();
    }
    if (store && resolution.descendantsStable && !store->hasDirtyDescendant(key)) {
      if (std::optional<Size> const cached = store->cachedMeasure(key, constraints)) {
        return *cached;
      }
    }
    Element fallbackChild = store ? Element{Rectangle{}} : Element{value.body()};
    Element const& child = store ? *resolution.body : fallbackChild;
    ctx.beginCompositeBodySubtree(key);
    ctx.pushCompositeKeyTail(key);
    if (child.expandsBody()) {
      ComponentKey childBodyKey = key;
      childBodyKey.push_back(detail::compositeBodyLocalId());
      ctx.setMeasurementRootKey(std::move(childBodyKey));
    }
    if (store) {
      store->recordBodyConstraints(key, constraints);
    }
    Size const sz = child.measure(ctx, constraints, hints, textSystem);
    if (store) {
      store->recordMeasure(key, constraints, sz);
    }
    ctx.clearMeasurementRootKey();
    ctx.popCompositeKeyTail();
    return sz;
  } else {
    static_assert(alwaysFalse<C>,
                  "Component must provide either measure(MeasureContext, LayoutConstraints, LayoutHints, "
                  "TextSystem) or body().");
    return {};
  }
}

template<typename C>
Element::Element(C component)
    : impl_(std::make_unique<Model<C>>(std::move(component)))
    , measureId_(detail::nextElementMeasureId()) {}

template<typename T>
bool Element::is() const noexcept {
  return impl_ && impl_->modelType() == std::type_index(typeid(T));
}

template<typename T>
T const& Element::as() const {
  assert(is<T>());
  return *static_cast<T const*>(impl_->rawValuePtr());
}

inline bool Element::valueEquals(Element const& other) const noexcept {
  if (!impl_ || !other.impl_) {
    return !impl_ && !other.impl_;
  }
  return impl_->valueEquals(*other.impl_);
}

namespace detail {

inline bool interactionHandlersComparable(Element const& lhs, Element const& rhs) noexcept {
  detail::ElementModifiers const* lhsMods = lhs.modifiers();
  detail::ElementModifiers const* rhsMods = rhs.modifiers();
  if (!lhsMods || !rhsMods) {
    return lhsMods == rhsMods;
  }
  return !lhsMods->onTap && !rhsMods->onTap &&
         !lhsMods->onPointerDown && !rhsMods->onPointerDown &&
         !lhsMods->onPointerUp && !rhsMods->onPointerUp &&
         !lhsMods->onPointerMove && !rhsMods->onPointerMove &&
         !lhsMods->onScroll && !rhsMods->onScroll &&
         !lhsMods->onKeyDown && !rhsMods->onKeyDown &&
         !lhsMods->onKeyUp && !rhsMods->onKeyUp &&
         !lhsMods->onTextInput && !rhsMods->onTextInput;
}

inline bool modifiersStructurallyEqual(Element const& lhs, Element const& rhs) noexcept {
  detail::ElementModifiers const* lhsMods = lhs.modifiers();
  detail::ElementModifiers const* rhsMods = rhs.modifiers();
  if (!lhsMods || !rhsMods) {
    return lhsMods == rhsMods;
  }
  if (!interactionHandlersComparable(lhs, rhs)) {
    return false;
  }
  if (lhsMods->padding != rhsMods->padding || lhsMods->fill != rhsMods->fill ||
      lhsMods->stroke != rhsMods->stroke || lhsMods->shadow != rhsMods->shadow ||
      lhsMods->cornerRadius != rhsMods->cornerRadius || lhsMods->opacity != rhsMods->opacity ||
      lhsMods->translation != rhsMods->translation || lhsMods->clip != rhsMods->clip ||
      lhsMods->positionX != rhsMods->positionX || lhsMods->positionY != rhsMods->positionY ||
      lhsMods->sizeWidth != rhsMods->sizeWidth || lhsMods->sizeHeight != rhsMods->sizeHeight ||
      lhsMods->focusable != rhsMods->focusable || lhsMods->cursor != rhsMods->cursor) {
    return false;
  }
  if (static_cast<bool>(lhsMods->overlay) != static_cast<bool>(rhsMods->overlay)) {
    return false;
  }
  if (lhsMods->overlay && !lhsMods->overlay->structuralEquals(*rhsMods->overlay)) {
    return false;
  }
  return true;
}

inline bool environmentStructurallyEqual(Element const& lhs, Element const& rhs) noexcept {
  EnvironmentLayer const* lhsEnv = lhs.environmentLayer();
  EnvironmentLayer const* rhsEnv = rhs.environmentLayer();
  if (!lhsEnv || !rhsEnv) {
    return lhsEnv == rhsEnv;
  }
  // Environment layers use erased std::any storage. Until values become comparably snapshottable,
  // only treat empty layers as structurally equal.
  return lhsEnv->empty() && rhsEnv->empty();
}

} // namespace detail

inline bool Element::structuralEquals(Element const& other) const noexcept {
  return valueEquals(other) &&
         flexGrowOverride_ == other.flexGrowOverride_ &&
         flexShrinkOverride_ == other.flexShrinkOverride_ &&
         flexBasisOverride_ == other.flexBasisOverride_ &&
         minMainSizeOverride_ == other.minMainSizeOverride_ &&
         key_ == other.key_ &&
         detail::environmentStructurallyEqual(*this, other) &&
         detail::modifiersStructurallyEqual(*this, other);
}

inline bool elementsStructurallyEqual(std::vector<Element> const& lhs,
                                      std::vector<Element> const& rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (!lhs[i].structuralEquals(rhs[i])) {
      return false;
    }
  }
  return true;
}

template<typename... Args>
std::vector<Element> children(Args&&... args) {
  std::vector<Element> v;
  v.reserve(sizeof...(args));
  (v.emplace_back(std::forward<Args>(args)), ...);
  return v;
}

} // namespace flux
