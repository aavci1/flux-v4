#pragma once

/// \file Flux/Detail/RootHolder.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/LayoutContext.hpp>
#include <Flux/UI/Component.hpp>
#include <Flux/UI/Element.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace flux {

/// Heap-allocated owner for the root component. Keeps the component at a stable address
/// for the lifetime of the window.
struct RootHolder {
  virtual ~RootHolder() = default;
  virtual void layoutInto(LayoutContext& ctx) const = 0;
};

template<typename C>
struct TypedRootHolder final : RootHolder {
  C value;

  explicit TypedRootHolder(std::in_place_t) : value{} {}
  explicit TypedRootHolder(std::in_place_t, C const& c) : value(c) {}
  explicit TypedRootHolder(std::in_place_t, C&& c) : value(std::move(c)) {}

  void layoutInto(LayoutContext& ctx) const override {
    if constexpr (CompositeComponent<C>) {
      ctx.pushChildIndex();
      ComponentKey const key = ctx.nextCompositeKey();
      StateStore* store = StateStore::current();
      if (store) {
        store->pushComponent(key, std::type_index(typeid(C)));
      }
      Element& child = ctx.pinElement(Element{value.body()});
      if (store) {
        store->popComponent();
      }
      ctx.beginCompositeBodySubtree(key);
      ctx.pushCompositeKeyTail(key);
      child.layout(ctx);
      ctx.popCompositeKeyTail();
      ctx.popChildIndex();
    } else {
      static_assert(std::is_copy_constructible_v<C>,
          "Leaf root component must be copy-constructible. "
          "If C has Signal/Animated members, give it a body() method.");
      Element& leaf = ctx.pinElement(Element{value});
      leaf.layout(ctx);
    }
  }
};

} // namespace flux
