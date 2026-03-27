#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include "Core/PlatformWindow.hpp"
#include "Graphics/CoreTextSystem.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#if FLUX_ENABLE_DEFAULT_EVENT_LOGGING
#include <cstdio>
#endif
#include <cmath>
#include <stdexcept>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>

namespace flux {

namespace {

Application* gCurrentApplication = nullptr;

#if FLUX_ENABLE_DEFAULT_EVENT_LOGGING

void logWindowEvent(WindowEvent const& e) {
  const char* kind = "?";
  switch (e.kind) {
  case WindowEvent::Kind::Resize:
    kind = "Resize";
    break;
  case WindowEvent::Kind::FocusGained:
    kind = "FocusGained";
    break;
  case WindowEvent::Kind::FocusLost:
    kind = "FocusLost";
    break;
  case WindowEvent::Kind::DpiChanged:
    kind = "DpiChanged";
    break;
  case WindowEvent::Kind::CloseRequest:
    kind = "CloseRequest";
    break;
  }
  std::printf("[flux] WindowEvent %s handle=%u size=%gx%g dpi=%g\n", kind, e.handle,
              static_cast<double>(e.size.width), static_cast<double>(e.size.height),
              static_cast<double>(e.dpi));
}

void logTimerEvent(TimerEvent const& e) {
  std::printf("[flux] TimerEvent deadlineNanos=%lld timerId=%llu windowHandle=%u\n",
              static_cast<long long>(e.deadlineNanos), static_cast<unsigned long long>(e.timerId),
              static_cast<unsigned>(e.windowHandle));
}

void logInputEvent(InputEvent const& e) {
  const char* kind = "?";
  switch (e.kind) {
  case InputEvent::Kind::PointerMove:
    kind = "PointerMove";
    break;
  case InputEvent::Kind::PointerDown:
    kind = "PointerDown";
    break;
  case InputEvent::Kind::PointerUp:
    kind = "PointerUp";
    break;
  case InputEvent::Kind::Scroll:
    kind = "Scroll";
    break;
  case InputEvent::Kind::KeyDown:
    kind = "KeyDown";
    break;
  case InputEvent::Kind::KeyUp:
    kind = "KeyUp";
    break;
  case InputEvent::Kind::TextInput:
    kind = "TextInput";
    break;
  case InputEvent::Kind::TouchBegin:
    kind = "TouchBegin";
    break;
  case InputEvent::Kind::TouchMove:
    kind = "TouchMove";
    break;
  case InputEvent::Kind::TouchEnd:
    kind = "TouchEnd";
    break;
  }
  std::printf("[flux] InputEvent %s handle=%u pos=(%g,%g) key=%u text=%s\n", kind, e.handle,
              static_cast<double>(e.position.x), static_cast<double>(e.position.y),
              static_cast<unsigned>(e.key), e.text.c_str());
}

#endif // FLUX_ENABLE_DEFAULT_EVENT_LOGGING

} // namespace

struct Application::Impl {
  struct RepeatingTimerState {
    std::uint64_t id = 0;
    std::chrono::steady_clock::time_point nextFire{};
    std::chrono::nanoseconds interval{};
    unsigned int windowHandle = 0;
  };

  Application* owner_{nullptr};
  std::unique_ptr<EventQueue> eventQueue_;
  std::unique_ptr<TextSystem> textSystem_;
  std::vector<std::unique_ptr<Window>> ownedWindows_;
  std::vector<Window*> windows_;
  std::atomic<bool> needsRedraw_{false};
  bool running_{true};
  std::atomic<std::uint64_t> nextTimerId_{0};
  std::vector<RepeatingTimerState> repeatingTimers_{};

  void shutdownTimers() { repeatingTimers_.clear(); }

  void wakePlatformWaitIfNeeded() {
    if (windows_.empty()) {
      return;
    }
    if (PlatformWindow* pw = windows_.front()->platformWindow()) {
      pw->wakeEventLoop();
    }
  }

  /// Next `waitForEvents` timeout, or -1 if there are no timers (infinite wait).
  int timerWaitTimeoutMs() const {
    if (repeatingTimers_.empty()) {
      return -1;
    }
    auto const now = std::chrono::steady_clock::now();
    for (auto const& t : repeatingTimers_) {
      if (t.nextFire <= now) {
        return 0;
      }
    }
    std::chrono::nanoseconds minRemain{};
    bool first = true;
    for (auto const& t : repeatingTimers_) {
      auto const remain = std::chrono::duration_cast<std::chrono::nanoseconds>(t.nextFire - now);
      if (first) {
        minRemain = remain;
        first = false;
      } else {
        minRemain = std::min(minRemain, remain);
      }
    }
    double const ms = std::chrono::duration<double, std::milli>(minRemain).count();
    int const msInt = static_cast<int>(std::ceil(ms));
    return std::max(msInt, 1);
  }

  void processDueTimers() {
    if (repeatingTimers_.empty() || !eventQueue_) {
      return;
    }
    auto const now = std::chrono::steady_clock::now();
    for (auto& t : repeatingTimers_) {
      if (now >= t.nextFire) {
        TimerEvent te{};
        te.deadlineNanos = static_cast<std::int64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
        te.timerId = t.id;
        te.windowHandle = t.windowHandle;
        eventQueue_->post(te);
        t.nextFire = now + t.interval;
      }
    }
  }

  std::uint64_t addRepeatingTimer(std::chrono::nanoseconds interval, unsigned int windowHandle) {
    const std::uint64_t id = nextTimerId_.fetch_add(1, std::memory_order_relaxed) + 1;
    RepeatingTimerState state{};
    state.id = id;
    state.interval = interval;
    state.nextFire = std::chrono::steady_clock::now() + interval;
    state.windowHandle = windowHandle;
    repeatingTimers_.push_back(std::move(state));
    wakePlatformWaitIfNeeded();
    return id;
  }

  void cancelRepeatingTimer(std::uint64_t timerId) {
    if (timerId == 0) {
      return;
    }
    std::size_t const before = repeatingTimers_.size();
    std::erase_if(repeatingTimers_, [timerId](RepeatingTimerState const& t) { return t.id == timerId; });
    if (repeatingTimers_.size() != before) {
      wakePlatformWaitIfNeeded();
    }
  }

  Window* findWindowByHandle(unsigned int handle) {
    for (Window* cand : windows_) {
      if (cand && cand->handle() == handle) {
        return cand;
      }
    }
    return nullptr;
  }

  void registerWindow(Window* window) { windows_.push_back(window); }

  void unregisterWindowByHandle(unsigned int handle) {
    std::erase_if(windows_, [handle](Window* w) { return w && w->handle() == handle; });
  }

  void handleCloseRequest(Window* window) {
    if (!window) {
      return;
    }
    std::erase_if(windows_, [window](Window* w) { return w == window; });
    std::erase_if(ownedWindows_,
                  [window](std::unique_ptr<Window> const& w) { return w.get() == window; });

    if (windows_.empty() && owner_) {
      owner_->quit();
    }
  }

  void installEventHandlers() {
    EventQueue& q = *eventQueue_;

    q.on<WindowLifecycleEvent>([this](WindowLifecycleEvent const& e) {
      if (e.kind == WindowLifecycleEvent::Kind::Registered) {
        if (e.window && owner_) {
          owner_->onWindowRegistered(e.window);
        }
      } else {
        unregisterWindowByHandle(e.handle);
      }
    });

    q.on<WindowEvent>([this](WindowEvent const& e) {
      if (e.kind == WindowEvent::Kind::CloseRequest) {
        handleCloseRequest(findWindowByHandle(e.handle));
        return;
      }
      if (e.kind == WindowEvent::Kind::Resize) {
        Window* w = findWindowByHandle(e.handle);
        if (w) {
          Canvas& c = w->canvas();
          c.resize(static_cast<int>(std::lround(static_cast<double>(e.size.width))),
                   static_cast<int>(std::lround(static_cast<double>(e.size.height))));
        }
        if (owner_) {
          owner_->requestRedraw();
        }
#if FLUX_ENABLE_DEFAULT_EVENT_LOGGING
        logWindowEvent(e);
#endif
        return;
      }
      if (e.kind == WindowEvent::Kind::DpiChanged) {
        if (owner_) {
          owner_->requestRedraw();
        }
#if FLUX_ENABLE_DEFAULT_EVENT_LOGGING
        logWindowEvent(e);
#endif
        return;
      }
#if FLUX_ENABLE_DEFAULT_EVENT_LOGGING
      logWindowEvent(e);
#endif
    });

#if FLUX_ENABLE_DEFAULT_EVENT_LOGGING
    q.on<InputEvent>([](InputEvent const& e) { logInputEvent(e); });

    q.on<TimerEvent>([](TimerEvent const& e) { logTimerEvent(e); });

    q.on<CustomEvent>([](CustomEvent const& e) {
      std::printf("[flux] CustomEvent type=%u\n", static_cast<unsigned>(e.type));
    });
#endif
  }
};

Application::Application(int /*argc*/, char** /*argv*/) {
  if (gCurrentApplication) {
    throw std::runtime_error("Application already exists");
  }
  gCurrentApplication = this;
  d = std::make_unique<Impl>();
  d->owner_ = this;
  d->eventQueue_ = std::unique_ptr<EventQueue>(new EventQueue());
  d->textSystem_ = std::make_unique<CoreTextSystem>();
  d->installEventHandlers();

  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp finishLaunching];
}

Application::~Application() {
  if (d) {
    d->shutdownTimers();
  }
  d->ownedWindows_.clear();
  d.reset();
  if (gCurrentApplication == this) {
    gCurrentApplication = nullptr;
  }
}

void Application::adoptOwnedWindow(std::unique_ptr<Window> window) {
  d->ownedWindows_.push_back(std::move(window));
}

EventQueue& Application::eventQueue() { return *d->eventQueue_; }

TextSystem& Application::textSystem() { return *d->textSystem_; }

bool Application::hasInstance() { return gCurrentApplication != nullptr; }

void Application::requestRedraw() {
  if (!d) {
    return;
  }
  d->needsRedraw_.store(true, std::memory_order_relaxed);
  if (!d->windows_.empty()) {
    if (PlatformWindow* pw = d->windows_.front()->platformWindow()) {
      pw->wakeEventLoop();
    }
  }
}

std::uint64_t Application::scheduleRepeatingTimer(std::chrono::nanoseconds interval, unsigned int windowHandle) {
  if (!d) {
    return 0;
  }
  if (interval.count() <= 0) {
    throw std::invalid_argument("scheduleRepeatingTimer: interval must be positive");
  }
  return d->addRepeatingTimer(interval, windowHandle);
}

void Application::cancelTimer(std::uint64_t timerId) {
  if (!d) {
    return;
  }
  d->cancelRepeatingTimer(timerId);
}

void Application::onWindowRegistered(Window* window) {
  if (!d || !window) {
    return;
  }
  d->registerWindow(window);
  if (PlatformWindow* pw = window->platformWindow()) {
    pw->show();
  }
  requestRedraw();
}

void Application::unregisterWindowHandle(unsigned int handle) {
  if (!d) {
    return;
  }
  d->unregisterWindowByHandle(handle);
}

int Application::exec() {
  [NSApp activateIgnoringOtherApps:YES];

  d->eventQueue_->dispatch();

  while (d->running_) {
    const bool redrawPending = d->needsRedraw_.load(std::memory_order_relaxed);

    if (!d->windows_.empty()) {
      Window* w = d->windows_.front();
      if (PlatformWindow* pw = w->platformWindow()) {
        if (redrawPending) {
          pw->processEvents();
        } else {
          const int timeoutMs = d->timerWaitTimeoutMs();
          pw->waitForEvents(timeoutMs);
        }
      }
    } else {
      if (d->running_) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
      }
    }

    d->processDueTimers();
    d->eventQueue_->dispatch();

    if (d->needsRedraw_.exchange(false, std::memory_order_relaxed)) {
      for (Window* win : d->windows_) {
        if (!win) {
          continue;
        }
        Canvas& c = win->canvas();
        c.beginFrame();
        win->render(c);
        c.present();
      }
    }
  }
  return 0;
}

void Application::quit() {
  d->running_ = false;
  if (!d->windows_.empty()) {
    if (PlatformWindow* pw = d->windows_.front()->platformWindow()) {
      pw->wakeEventLoop();
    }
  }
}

Application& Application::instance() {
  if (!gCurrentApplication) {
    throw std::runtime_error("Application not initialized");
  }
  return *gCurrentApplication;
}

} // namespace flux
