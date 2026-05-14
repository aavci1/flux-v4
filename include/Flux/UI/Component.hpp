#pragma once

/// \file Flux/UI/Component.hpp
///
/// Part of the Flux public API.

#include <concepts>
#include <type_traits>

namespace flux {

class Canvas;
class MeasureContext;
class TextSystem;

template<typename T>
concept BodyComponent = requires(T const& t) {
  { t.body() };
};

template<typename T>
concept Component = true;

} // namespace flux
