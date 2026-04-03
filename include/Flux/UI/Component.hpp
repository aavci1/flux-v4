#pragma once

/// \file Flux/UI/Component.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <concepts>

namespace flux {

class Canvas;
class LayoutContext;
class RenderContext;
struct LayoutNode;
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
    requires(T const& t, LayoutContext& lctx, RenderContext& rctx, LayoutNode const& node,
             LayoutConstraints const& c, LayoutHints const& h, TextSystem& ts) {
      { t.layout(lctx) };
      { t.measure(lctx, c, h, ts) } -> std::convertible_to<Size>;
      { t.renderFromLayout(rctx, node) };
    } && !CompositeComponent<T> && !RenderComponent<T>;

} // namespace flux
