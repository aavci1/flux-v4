#pragma once

/// \file Flux/UI/Views/ForEach.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Alignment.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <functional>
#include <vector>

namespace flux {

/// Transparent element expander for dynamic lists.
///
/// `ForEach` lowers to a `VStack` body during retained-scene reconciliation; per-item stability
/// comes from explicit child keys returned by `factory`.
template<typename T>
struct ForEach {
  std::vector<T> items;
  std::function<Element(T const&)> factory;
  float spacing = 0.f;
  Alignment alignment = Alignment::Start;

  ForEach(std::vector<T> itemsIn, std::function<Element(T const&)> factoryIn, float spacingIn = 0.f)
      : items(std::move(itemsIn)), factory(std::move(factoryIn)), spacing(spacingIn) {}

  Element body() const {
    std::vector<Element> children;
    children.reserve(items.size());
    for (T const& item : items) {
      children.push_back(factory(item));
    }
    return Element{VStack{
        .spacing = spacing,
        .alignment = alignment,
        .children = std::move(children),
    }};
  }
};

} // namespace flux
