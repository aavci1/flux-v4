#pragma once

#include <Flux/Core/Window.hpp>
#include <Flux/UI/Element.hpp>

namespace flux {

template<typename C>
void Window::setView(C component) {
  setViewRoot(Element(std::move(component)));
}

} // namespace flux
