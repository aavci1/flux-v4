#pragma once

/// \file Flux/Core/Application.hpp
///
/// Part of the Flux public API.


#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <Flux/Core/Clipboard.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Reactive/Observer.hpp>

namespace flux {

class EventQueue;
class TextSystem;

/// Coalesced main-loop work: `Rebuild` implies a repaint after reactive callbacks run.
enum class FrameRequest : std::uint8_t { None = 0, Repaint = 1, Rebuild = 2 };

constexpr bool operator>=(FrameRequest a, FrameRequest b) noexcept {
  return static_cast<std::uint8_t>(a) >= static_cast<std::uint8_t>(b);
}

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

  /// Marks all windows for a render pass on the next `exec()` iteration and wakes the platform event wait
  /// (unless already inside `EventQueue::dispatch`).
  void requestRepaint();

  /// Schedules reactive rebuild (`onNextFrameNeeded` callbacks) and wakes the event wait when not dispatching.
  void requestRebuild();

  /// Presents immediately when a repaint or rebuild is pending. Coalescing can leave `Rebuild`
  /// after `requestRepaint`; that case still presents (e.g. live resize) while preserving `Rebuild`
  /// for the next `exec()` iteration when reactive work remains.
  void flushPending();

  /// Repeating timer using `std::chrono::steady_clock` in the main `exec()` loop; posts `TimerEvent` each tick.
  /// Returns an id for `cancelTimer`. `windowHandle` is optional metadata for handlers (e.g. redraw routing).
  std::uint64_t scheduleRepeatingTimer(std::chrono::nanoseconds interval, unsigned int windowHandle = 0);
  void cancelTimer(std::uint64_t timerId);

  static bool hasInstance();
  static Application& instance();

  EventQueue& eventQueue();

  TextSystem& textSystem();

  /// Returns the process-wide system clipboard.
  /// The returned reference is valid for the lifetime of the Application.
  Clipboard& clipboard();

  /// Opaque platform font handle for the bundled icon font (on macOS: `CTFontRef`), or null if load failed.
  void* iconFontHandle() const;

  /// Batched callback: runs at most once per `exec()` iteration after any reactive update.
  ObserverHandle onNextFrameNeeded(std::function<void()> callback);
  void unobserveNextFrame(ObserverHandle handle);

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
