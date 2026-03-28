#pragma once

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Component.hpp>
#include <Flux/UI/Element.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace flux {

/// Heap-allocated owner for the root component. Keeps the component at a stable address
/// so Signal/Animated members remain valid across rebuilds.
struct RootHolder {
  virtual ~RootHolder() = default;
  virtual void buildInto(BuildContext& ctx) const = 0;
};

template<typename C>
struct TypedRootHolder final : RootHolder {
  C value;

  explicit TypedRootHolder(std::in_place_t) : value{} {}
  explicit TypedRootHolder(std::in_place_t, C const& c) : value(c) {}
  explicit TypedRootHolder(std::in_place_t, C&& c) : value(std::move(c)) {}

  void buildInto(BuildContext& ctx) const override {
    if constexpr (CompositeComponent<C>) {
      Element child{value.body()};
      child.build(ctx);
    } else {
      static_assert(std::is_copy_constructible_v<C>,
          "Leaf root component must be copy-constructible. "
          "If C has Signal/Animated members, give it a body() method.");
      Element leaf{value};
      leaf.build(ctx);
    }
  }
};

} // namespace flux
