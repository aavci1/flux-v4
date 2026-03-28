#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <Flux/Core/Window.hpp>
#include <Flux/Reactive/Observer.hpp>

namespace flux {

class EventQueue;
class TextSystem;

class Application {
public:
  explicit Application(int argc = 0, char** argv = nullptr);
  ~Application();

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(Application&&) = delete;

  template<typename T = Window, typename... Args>
  T& createWindow(WindowConfig const& config, Args&&... args) {
    static_assert(std::is_base_of_v<Window, T>);
    // `new T` must run in a member of `Application` so `friend Application` can call `Window`'s protected ctor.
    auto window = std::unique_ptr<T>(new T(config, std::forward<Args>(args)...));
    T* raw = window.get();
    adoptOwnedWindow(std::move(window));
    return *raw;
  }

  int exec();
  void quit();

  /// Marks all windows for a render pass on the next `exec()` iteration and wakes the platform event wait.
  void requestRedraw();

  /// Presents pending frames immediately. Use when the main loop may not iterate (e.g. live window resize runs
  /// the run loop in `NSEventTrackingRunLoopMode`, so `waitForEvents` in `NSDefaultRunLoopMode` does not return).
  void flushRedraw();

  /// Repeating timer using `std::chrono::steady_clock` in the main `exec()` loop; posts `TimerEvent` each tick.
  /// Returns an id for `cancelTimer`. `windowHandle` is optional metadata for handlers (e.g. redraw routing).
  std::uint64_t scheduleRepeatingTimer(std::chrono::nanoseconds interval, unsigned int windowHandle = 0);
  void cancelTimer(std::uint64_t timerId);

  static bool hasInstance();
  static Application& instance();

  EventQueue& eventQueue();

  TextSystem& textSystem();

  /// Batched callback: runs at most once per `exec()` iteration after any reactive update.
  ObserverHandle onNextFrameNeeded(std::function<void()> callback);
  void unobserveNextFrame(ObserverHandle handle);

  /// Marks that reactive state changed; wakes the event loop. Used internally by the reactive layer.
  void markReactiveDirty();

  friend class Window;

private:
  void adoptOwnedWindow(std::unique_ptr<Window> window);
  /// Invoked when `WindowLifecycleEvent::Registered` is dispatched (first `exec()` `dispatch()` drains the ctor post).
  void onWindowRegistered(Window* window);
  /// Removes `handle` from the running window list before `Window` is destroyed (synchronous; avoids dangling `Window*`).
  void unregisterWindowHandle(unsigned int handle);

  void presentAllWindows();

  struct Impl;
  std::unique_ptr<Impl> d;
};

} // namespace flux
