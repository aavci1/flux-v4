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
  bool isComposite() const noexcept override { return CompositeComponent<C>; }
  std::unique_ptr<Element> buildCompositeBody() const override {
    if constexpr (CompositeComponent<C>) {
      return std::make_unique<Element>(value.body());
    }
    return nullptr;
  }
  detail::CompositeBodyResolution resolveCompositeBody(ComponentKey const& key,
                                                       LayoutConstraints const& constraints,
                                                       detail::ElementModifiers const* modifiers) const override;
  Size measure(MeasureContext& ctx, LayoutConstraints const& constraints, LayoutHints const& hints,
               TextSystem& textSystem) const override;
  float flexGrow() const override { return detail::flexGrowOf(value); }
  float flexShrink() const override { return detail::flexShrinkOf(value); }
  float minMainSize() const override { return detail::minMainSizeOf(value); }
  bool leafDrawsFillStrokeShadowFromModifiers() const override {
    if constexpr (std::is_same_v<C, Rectangle> || std::is_same_v<C, PathShape>) {
      return true;
    }
    return false;
  }
};

template<typename C>
detail::CompositeBodyResolution Element::Model<C>::resolveCompositeBody(ComponentKey const& key,
                                                                        LayoutConstraints const& constraints,
                                                                        detail::ElementModifiers const* modifiers) const {
  if constexpr (!CompositeComponent<C>) {
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
  } else if constexpr (std::is_same_v<C, Render>) {
    return ElementType::Render;
  } else if constexpr (std::is_same_v<C, views::Image>) {
    return ElementType::Image;
  } else if constexpr (std::is_same_v<C, PathShape>) {
    return ElementType::Path;
  } else if constexpr (std::is_same_v<C, Line>) {
    return ElementType::Line;
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
  if constexpr (CompositeComponent<C>) {
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
    if (store) {
      store->recordBodyConstraints(key, constraints);
      store->pushCompositePathStable(resolution.descendantsStable);
    }
    Size const sz = child.measure(ctx, constraints, hints, textSystem);
    if (store) {
      store->recordMeasure(key, constraints, sz);
      store->popCompositePathStable();
    }
    ctx.popCompositeKeyTail();
    return sz;
  } else if constexpr (PrimitiveComponent<C>) {
    return value.measure(ctx, constraints, hints, textSystem);
  } else {
    static_assert(alwaysFalse<C>,
                  "Component must satisfy CompositeComponent (body()) or PrimitiveComponent (measure with "
                  "MeasureContext).");
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

template<typename... Args>
std::vector<Element> children(Args&&... args) {
  std::vector<Element> v;
  v.reserve(sizeof...(args));
  (v.emplace_back(std::forward<Args>(args)), ...);
  return v;
}

} // namespace flux
