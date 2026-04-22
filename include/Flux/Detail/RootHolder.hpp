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
  if constexpr (ExpandsBodyComponent<C>) {
    return std::monostate{};
  } else {
    return Element{value};
  }
}

} // namespace detail

/// Heap-allocated owner for the root component. Keeps the component at a stable address
/// for the lifetime of the window.
struct ResolvedRootScene {
  Element const* element = nullptr;
  ComponentKey rootKey{};
  bool descendantsStable = false;
};

struct RootHolder {
  virtual ~RootHolder() = default;
  [[nodiscard]] virtual ResolvedRootScene resolveScene(LayoutConstraints const& constraints) const = 0;
};

template<typename C>
struct TypedRootHolder final : RootHolder {
  C value;
  [[no_unique_address]] std::conditional_t<ExpandsBodyComponent<C>, std::monostate, Element> cachedLeaf_;
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

  [[nodiscard]] ResolvedRootScene resolveScene(LayoutConstraints const& constraints) const override {
    ComponentKey const key = sceneRootKey();
    if constexpr (ExpandsBodyComponent<C>) {
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
        sceneDescendantsStable_ = resolution.descendantsStable;
        return ResolvedRootScene{
            .element = store->cachedBody(key),
            .rootKey = std::move(key),
            .descendantsStable = sceneDescendantsStable_,
        };
      }
      sceneDescendantsStable_ = resolution.descendantsStable;
      return ResolvedRootScene{
          .element = nullptr,
          .rootKey = std::move(key),
          .descendantsStable = sceneDescendantsStable_,
      };
    } else {
      sceneDescendantsStable_ = false;
      return ResolvedRootScene{
          .element = &cachedLeaf_,
          .rootKey = std::move(key),
          .descendantsStable = false,
      };
    }
  }

private:
  [[nodiscard]] ComponentKey sceneRootKey() const noexcept {
    if constexpr (ExpandsBodyComponent<C>) {
      return ComponentKey{LocalId::fromIndex(0), LocalId::fromIndex(0)};
    } else {
      return ComponentKey{LocalId::fromIndex(0)};
    }
  }
};

} // namespace flux
