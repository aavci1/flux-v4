#pragma once

/// \file Flux/UI/Component.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <concepts>

namespace flux {

class Canvas;

template<typename T>
concept CompositeComponent = requires(T const& t) {
  { t.body() };
};

template<typename T>
concept LeafComponent = !CompositeComponent<T>;

template<typename T>
concept Component = true;

template<typename T>
concept RenderComponent = requires(T const& t, Canvas& c, Rect r, LayoutConstraints const& cs) {
  { t.render(c, r) };
  { t.measure(cs) } -> std::convertible_to<Size>;
} && !CompositeComponent<T>;

} // namespace flux
