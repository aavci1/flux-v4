#pragma once

#include <Flux/Core/Events.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>

#include <optional>

namespace flux {

class Window;
class LayoutEngine;

/// Internal driver for `Window::setView` — not part of the stable public API surface.
class Runtime {
public:
  explicit Runtime(Window& window);
  ~Runtime();

  template<typename C>
  void setView(C component) {
    root_.emplace(std::move(component));
    rebuild();
  }

  void handleInput(InputEvent const& e);

private:
  void rebuild();
  void subscribeToRebuild();
  void subscribeInput();

  Window& window_;
  std::optional<Element> root_;
  EventMap eventMap_;
  LayoutEngine layoutEngine_;
  ObserverHandle rebuildHandle_{};
  bool inputRegistered_ = false;
};

} // namespace flux
