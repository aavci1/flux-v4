#pragma once

/// \file Flux/UI/Component.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <concepts>

namespace flux {

class Canvas;
class MeasureContext;
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
concept PrimitiveComponent =
    requires(T const& t, MeasureContext& mctx, LayoutConstraints const& c, LayoutHints const& h, TextSystem& ts) {
      { t.measure(mctx, c, h, ts) } -> std::convertible_to<Size>;
    } && !CompositeComponent<T>;

} // namespace flux
