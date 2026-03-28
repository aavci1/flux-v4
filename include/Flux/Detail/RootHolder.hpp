#pragma once

#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Component.hpp>
#include <Flux/UI/Element.hpp>

#include <memory>
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

  template<typename... Args>
  explicit TypedRootHolder(std::in_place_t, Args&&... args) : value(std::forward<Args>(args)...) {}

  void buildInto(BuildContext& ctx) const override {
    if constexpr (CompositeComponent<C>) {
      Element child{value.body()};
      child.build(ctx);
    } else {
      Element leaf{value};
      leaf.build(ctx);
    }
  }
};

} // namespace flux
