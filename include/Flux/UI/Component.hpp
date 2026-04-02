#pragma once

/// \file Flux/UI/Component.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <concepts>

namespace flux {

class Canvas;
class BuildContext;
class TextSystem;

template<typename T>
concept CompositeComponent = requires(T const& t) {
  { t.body() };
};

template<typename T>
concept LeafComponent = !CompositeComponent<T>;

template<typename T>
concept Component = true;

template<typename T>
concept RenderComponent = requires(T const& t, Canvas& c, Rect r, LayoutConstraints const& cs,
                                 LayoutHints const& h) {
  { t.render(c, r) };
  { t.measure(cs, h) } -> std::convertible_to<Size>;
} && !CompositeComponent<T>;

template<typename T>
concept PrimitiveComponent =
    requires(T const& t, BuildContext& ctx, LayoutConstraints const& c, LayoutHints const& h,
             TextSystem& ts) {
      { t.build(ctx) };
      { t.measure(ctx, c, h, ts) } -> std::convertible_to<Size>;
    } && !CompositeComponent<T> && !RenderComponent<T>;

} // namespace flux
