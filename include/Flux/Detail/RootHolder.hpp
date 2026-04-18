#pragma once

/// \file Flux/Detail/RootHolder.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Component.hpp>
#include <Flux/UI/Element.hpp>

#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

namespace flux {

namespace detail {

template<typename C>
auto makeCachedLeaf(C const& value) {
  if constexpr (CompositeComponent<C>) {
    return std::monostate{};
  } else {
    return Element{value};
  }
}

} // namespace detail

/// Heap-allocated owner for the root component. Keeps the component at a stable address
/// for the lifetime of the window.
struct RootHolder {
  virtual ~RootHolder() = default;
  virtual void prepareSceneElement(LayoutConstraints const& constraints) const = 0;
  [[nodiscard]] virtual ComponentKey sceneRootKey() const noexcept = 0;
  [[nodiscard]] virtual Element const* sceneElementForCurrentBuild() const noexcept = 0;
  [[nodiscard]] virtual bool sceneElementDescendantsStable() const noexcept = 0;
};

template<typename C>
struct TypedRootHolder final : RootHolder {
  C value;
  [[no_unique_address]] std::conditional_t<CompositeComponent<C>, std::monostate, Element> cachedLeaf_;
  mutable bool sceneDescendantsStable_ = false;

  explicit TypedRootHolder(std::in_place_t)
      : value{}
      , cachedLeaf_(detail::makeCachedLeaf(value)) {}
  explicit TypedRootHolder(std::in_place_t, C const& c)
      : value(c)
      , cachedLeaf_(detail::makeCachedLeaf(value)) {}
  explicit TypedRootHolder(std::in_place_t, C&& c)
      : value(std::move(c))
      , cachedLeaf_(detail::makeCachedLeaf(value)) {}

  void prepareSceneElement(LayoutConstraints const& constraints) const override {
    if constexpr (CompositeComponent<C>) {
      ComponentKey const key = sceneRootKey();
      StateStore* store = StateStore::current();
      detail::CompositeBodyResolution resolution{};
      if (store) {
        store->pushComponent(key, std::type_index(typeid(C)));
        store->pushCompositeConstraints(constraints);
        try {
          resolution = detail::resolveCompositeBody(store, key, constraints, value,
                                                    [&] { return value.body(); });
        } catch (...) {
          store->popCompositeConstraints();
          store->popComponent();
          throw;
        }
        store->popCompositeConstraints();
        store->popComponent();
      }
      if (store) {
        store->recordBodyConstraints(key, constraints);
      }
      sceneDescendantsStable_ = resolution.descendantsStable;
    } else {
      sceneDescendantsStable_ = false;
    }
  }

  [[nodiscard]] Element const* sceneElementForCurrentBuild() const noexcept override {
    if constexpr (CompositeComponent<C>) {
      StateStore* const store = StateStore::current();
      if (!store) {
        return nullptr;
      }
      return store->cachedBody(sceneRootKey());
    } else {
      return &cachedLeaf_;
    }
  }

  [[nodiscard]] ComponentKey sceneRootKey() const noexcept override {
    if constexpr (CompositeComponent<C>) {
      return ComponentKey{LocalId::fromIndex(0), LocalId::fromIndex(0)};
    } else {
      return ComponentKey{LocalId::fromIndex(0)};
    }
  }

  [[nodiscard]] bool sceneElementDescendantsStable() const noexcept override {
    return sceneDescendantsStable_;
  }
};

} // namespace flux
