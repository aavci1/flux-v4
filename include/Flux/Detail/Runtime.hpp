#pragma once

#include <Flux/Core/Events.hpp>
#include <Flux/Detail/RootHolder.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>

#include <memory>
#include <optional>

namespace flux {

class Window;
class LayoutEngine;

/// Internal driver for `Window::setView` — not part of the stable public API surface.
class Runtime {
public:
  explicit Runtime(Window& window);
  ~Runtime();

  void setRoot(std::unique_ptr<RootHolder> holder);

  void handleInput(InputEvent const& e);

private:
  /// `sizeOverride` — size from `WindowEvent::size` on resize (preferred over re-querying the view).
  void rebuild(std::optional<Size> sizeOverride = std::nullopt);
  void rebuild(Size const& sizeFromResizeEvent) { rebuild(std::optional<Size>(sizeFromResizeEvent)); }
  void subscribeToRebuild();
  void subscribeInput();
  void subscribeResize();

  Window& window_;
  std::unique_ptr<RootHolder> rootHolder_;
  EventMap eventMap_;
  LayoutEngine layoutEngine_;
  ObserverHandle rebuildHandle_{};
  bool inputRegistered_ = false;
};

} // namespace flux
