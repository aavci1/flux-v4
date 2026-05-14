#pragma once

/// \file Flux/UI/WindowUI.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Window.hpp>
#include <Flux/UI/Detail/RootHolder.hpp>

#include <type_traits>

namespace flux {

template<typename C>
void Window::setView(C&& component) {
  setViewRoot(std::make_unique<TypedRootHolder<std::decay_t<C>>>(
      std::in_place, std::forward<C>(component)));
}

template<typename C>
void Window::setView() {
  setViewRoot(std::make_unique<TypedRootHolder<C>>(std::in_place));
}

} // namespace flux
