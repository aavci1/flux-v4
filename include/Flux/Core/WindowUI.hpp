#pragma once

/// \file Flux/Core/WindowUI.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Window.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Layout.hpp>
#include <Flux/UI/Overlay.hpp>

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
