#include <Flux/Core/Application.hpp>

#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Reactive/AnimationClock.hpp>

#include "Core/PlatformWindow.hpp"
#include "Graphics/CoreTextSystem.hpp"
#include "Platform/Mac/MacClipboard.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <dispatch/dispatch.h>

namespace flux {

namespace detail {

bool signalBridgeApplicationHasInstance() {
  return Application::hasInstance();
}

void signalBridgeMarkReactiveDirty() {
  Application::instance().markReactiveDirty();
}

} // namespace detail

namespace {

Application* gCurrent = nullptr;

} // namespace

struct Application::Impl {
  EventQueue eventQueue_;
  std::vector<std::unique_ptr<Window>> windows_;
  std::unordered_map<unsigned int, Window*> byHandle_;

  std::unique_ptr<CoreTextSystem> textSystem_;
  std::unique_ptr<Clipboard> clipboard_;

  /// `CTFontRef` retained; released in `Application` destructor.
  void* iconFont_ = nullptr;

  bool quit_ = false;
  bool redraw_ = false;
  bool reactiveDirty_ = false;

  struct NextFrameEntry {
    std::uint64_t id = 0;
    std::function<void()> callback;
  };
  std::vector<NextFrameEntry> nextFrame_;
  std::uint64_t nextFrameId_ = 1;

  struct TimerEntry {
    std::uint64_t id = 0;
    std::chrono::nanoseconds interval{};
    std::chrono::steady_clock::time_point next{};
    unsigned int windowHandle = 0;
  };
  std::vector<TimerEntry> timers_;
  std::uint64_t nextTimerId_ = 1;

  int nextTimerTimeoutMs() const {
    using namespace std::chrono;
    if (timers_.empty()) {
      return -1;
    }
    auto const now = steady_clock::now();
    auto minNext = timers_.front().next;
    for (auto const& t : timers_) {
      if (t.next < minNext) {
        minNext = t.next;
      }
    }
    if (minNext <= now) {
      return 0;
    }
    auto const ms = duration_cast<milliseconds>(minNext - now).count();
    return static_cast<int>(std::min<std::int64_t>(ms, static_cast<std::int64_t>(INT_MAX)));
  }
};

Application::Application(int /*argc*/, char** /*argv*/) {
  if (gCurrent) {
    throw std::runtime_error("Application already exists");
  }
  gCurrent = this;
  d = std::make_unique<Impl>();
  d->textSystem_ = std::make_unique<CoreTextSystem>();
  d->clipboard_ = std::make_unique<MacClipboard>();

  {
    NSBundle* const bundle = [NSBundle mainBundle];
    NSString* fontPath = [bundle pathForResource:@"MaterialSymbolsRounded" ofType:@"ttf" inDirectory:@"fonts"];
    if (!fontPath) {
      NSString* const bp = [bundle bundlePath];
      fontPath = [bp stringByAppendingPathComponent:@"fonts/MaterialSymbolsRounded.ttf"];
    }
    if (fontPath && [[NSFileManager defaultManager] fileExistsAtPath:fontPath]) {
      NSURL* const url = [NSURL fileURLWithPath:fontPath];
      CTFontManagerRegisterFontsForURL((__bridge CFURLRef)url, kCTFontManagerScopeProcess, nullptr);
      CTFontRef const ct = CTFontCreateWithName(CFSTR("Material Symbols Rounded"), 24., nullptr);
      d->iconFont_ = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(ct));
    }
  }

  d->eventQueue_.on<WindowLifecycleEvent>([this](WindowLifecycleEvent const& e) {
    if (e.kind == WindowLifecycleEvent::Kind::Registered && e.window != nullptr) {
      onWindowRegistered(e.window);
    }
  });

  d->eventQueue_.on<WindowEvent>([this](WindowEvent const& ev) {
    if (ev.kind != WindowEvent::Kind::CloseRequest) {
      return;
    }
    // Defer destruction until after all WindowEvent handlers for this event return. Otherwise handlers
    // registered after Application (e.g. per-window subscriptions) may run with a dangling `this`.
    unsigned int const closeHandle = ev.handle;
    Application* app = this;
    dispatch_async(dispatch_get_main_queue(), ^{
      auto it = std::find_if(app->d->windows_.begin(), app->d->windows_.end(),
                             [&](std::unique_ptr<Window> const& w) {
                               return w && w->handle() == closeHandle;
                             });
      if (it == app->d->windows_.end()) {
        return;
      }
      app->d->byHandle_.erase(closeHandle);
      app->d->windows_.erase(it);
      if (app->d->windows_.empty()) {
        app->quit();
      }
    });
  });

  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  AnimationClock::instance().install(d->eventQueue_);
}

Application::~Application() {
  if (d->iconFont_) {
    CFRelease(reinterpret_cast<CTFontRef>(reinterpret_cast<std::uintptr_t>(d->iconFont_)));
    d->iconFont_ = nullptr;
  }
  d->windows_.clear();
  if (gCurrent == this) {
    gCurrent = nullptr;
  }
}

void Application::adoptOwnedWindow(std::unique_ptr<Window> window) {
  Window* raw = window.get();
  unsigned int const h = raw->handle();
  d->windows_.push_back(std::move(window));
  d->byHandle_[h] = raw;
}

void Application::onWindowRegistered(Window* window) {
  if (!window) {
    return;
  }
  window->platformWindow()->show();
}

void Application::unregisterWindowHandle(unsigned int handle) { d->byHandle_.erase(handle); }

EventQueue& Application::eventQueue() { return d->eventQueue_; }

TextSystem& Application::textSystem() { return *d->textSystem_; }

Clipboard& Application::clipboard() { return *d->clipboard_; }

void* Application::iconFontHandle() const { return d->iconFont_; }

ObserverHandle Application::onNextFrameNeeded(std::function<void()> callback) {
  std::uint64_t const id = d->nextFrameId_++;
  d->nextFrame_.push_back(Impl::NextFrameEntry{id, std::move(callback)});
  return ObserverHandle{id};
}

void Application::unobserveNextFrame(ObserverHandle handle) {
  if (!handle.isValid()) {
    return;
  }
  std::erase_if(d->nextFrame_, [&](Impl::NextFrameEntry const& e) { return e.id == handle.id; });
}

void Application::markReactiveDirty() {
  d->reactiveDirty_ = true;
  for (auto& w : d->windows_) {
    if (w) {
      w->platformWindow()->wakeEventLoop();
    }
  }
}

void Application::requestRedraw() {
  d->redraw_ = true;
  for (auto& w : d->windows_) {
    if (w) {
      w->platformWindow()->wakeEventLoop();
    }
  }
}

void Application::presentAllWindows() {
  for (auto& w : d->windows_) {
    if (!w) {
      continue;
    }
    Canvas& canvas = w->canvas();
    canvas.beginFrame();
    w->render(canvas);
    canvas.present();
  }
}

void Application::flushRedraw() {
  if (!d->redraw_) {
    return;
  }
  d->redraw_ = false;
  presentAllWindows();
}

std::uint64_t Application::scheduleRepeatingTimer(std::chrono::nanoseconds interval, unsigned int windowHandle) {
  std::uint64_t const id = d->nextTimerId_++;
  Impl::TimerEntry t;
  t.id = id;
  t.interval = interval;
  t.windowHandle = windowHandle;
  t.next = std::chrono::steady_clock::now() + interval;
  d->timers_.push_back(std::move(t));
  return id;
}

void Application::cancelTimer(std::uint64_t timerId) {
  std::erase_if(d->timers_, [&](Impl::TimerEntry const& t) { return t.id == timerId; });
}

int Application::exec() {
  [NSApp activate];
  while (!d->quit_) {
    for (auto& w : d->windows_) {
      if (w) {
        w->platformWindow()->processEvents();
      }
    }

    {
      using namespace std::chrono;
      auto const now = steady_clock::now();
      for (auto& t : d->timers_) {
        if (now >= t.next) {
          TimerEvent te{};
          te.deadlineNanos = duration_cast<nanoseconds>(now.time_since_epoch()).count();
          te.timerId = t.id;
          te.windowHandle = t.windowHandle;
          d->eventQueue_.post(std::move(te));
          t.next = now + t.interval;
        }
      }
    }

    d->eventQueue_.dispatch();

    if (d->reactiveDirty_) {
      d->reactiveDirty_ = false;
      for (auto& e : d->nextFrame_) {
        if (e.callback) {
          e.callback();
        }
      }
    }

    if (d->redraw_) {
      d->redraw_ = false;
      presentAllWindows();
    }

    int timeoutMs = d->nextTimerTimeoutMs();
    if (d->redraw_ || d->reactiveDirty_) {
      timeoutMs = 0;
    }
    if (!d->windows_.empty()) {
      d->windows_[0]->platformWindow()->waitForEvents(timeoutMs);
    } else {
      if (timeoutMs < 0) {
        timeoutMs = 16;
      }
      NSDate* until = [NSDate dateWithTimeIntervalSinceNow:timeoutMs / 1000.0];
      [NSRunLoop.currentRunLoop runUntilDate:until];
    }
  }
  return 0;
}

void Application::quit() {
  d->quit_ = true;
  for (auto& w : d->windows_) {
    if (w) {
      w->platformWindow()->wakeEventLoop();
    }
  }
}

Application& Application::instance() {
  if (!gCurrent) {
    throw std::runtime_error("Application not initialized");
  }
  return *gCurrent;
}

bool Application::hasInstance() { return gCurrent != nullptr; }

} // namespace flux
