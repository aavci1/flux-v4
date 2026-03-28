#pragma once

#include <Flux/Core/Window.hpp>
#include <Flux/Detail/RootHolder.hpp>

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
