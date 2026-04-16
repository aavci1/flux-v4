#pragma once

/// \file Flux/Detail/RootHolder.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/Component.hpp>
#include <Flux/UI/Element.hpp>

#include <cstdint>
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
  virtual void layoutInto(LayoutContext& ctx) const = 0;
  [[nodiscard]] virtual std::uint64_t layoutIdentityToken() const noexcept = 0;
};

template<typename C>
struct TypedRootHolder final : RootHolder {
  C value;
  [[no_unique_address]] std::conditional_t<CompositeComponent<C>, std::monostate, Element> cachedLeaf_;

  explicit TypedRootHolder(std::in_place_t)
      : value{}
      , cachedLeaf_(detail::makeCachedLeaf(value)) {}
  explicit TypedRootHolder(std::in_place_t, C const& c)
      : value(c)
      , cachedLeaf_(detail::makeCachedLeaf(value)) {}
  explicit TypedRootHolder(std::in_place_t, C&& c)
      : value(std::move(c))
      , cachedLeaf_(detail::makeCachedLeaf(value)) {}

  void layoutInto(LayoutContext& ctx) const override {
    if constexpr (CompositeComponent<C>) {
      ctx.pushChildIndex();
      ComponentKey const key = ctx.nextCompositeKey();
      StateStore* store = StateStore::current();
      detail::CompositeBodyResolution resolution{};
      if (store) {
        store->pushComponent(key, std::type_index(typeid(C)));
        store->pushCompositeConstraints(ctx.constraints());
        try {
          resolution = detail::resolveCompositeBody(store, key, ctx.constraints(), value,
                                                    [&] { return value.body(); });
        } catch (...) {
          store->popCompositeConstraints();
          store->popComponent();
          throw;
        }
        store->popCompositeConstraints();
        store->popComponent();
      }
      Element& child = store ? *resolution.body : ctx.pinElement(Element{value.body()});
      ctx.beginCompositeBodySubtree(key);
      ctx.pushCompositeKeyTail(key);
      if (store) {
        store->recordBodyConstraints(key, ctx.constraints());
        store->pushCompositePathStable(resolution.descendantsStable);
      }
      child.layout(ctx);
      if (store) {
        store->popCompositePathStable();
      }
      ctx.popCompositeKeyTail();
      ctx.popChildIndex();
    } else {
      static_assert(std::is_copy_constructible_v<C>,
          "Leaf root component must be copy-constructible. "
          "If C has Signal/Animation members, give it a body() method.");
      cachedLeaf_.layout(ctx);
    }
  }

  [[nodiscard]] std::uint64_t layoutIdentityToken() const noexcept override {
    if constexpr (CompositeComponent<C>) {
      return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&value));
    } else {
      return cachedLeaf_.measureId();
    }
  }
};

} // namespace flux
