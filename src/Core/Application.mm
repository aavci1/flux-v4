#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Graphics/Canvas.hpp>

#include <algorithm>
#include <cmath>
#if FLUX_ENABLE_DEFAULT_EVENT_LOGGING
#include <cstdio>
#endif
#include <stdexcept>
#include <vector>

#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CoreFoundation.h>

namespace flux {

namespace {

Application* gCurrentApplication = nullptr;

#if FLUX_ENABLE_DEFAULT_EVENT_LOGGING

void logWindowEvent(WindowEvent const& e) {
  if (e.kind == WindowEvent::Kind::CloseRequest || e.kind == WindowEvent::Kind::Redraw) {
    return;
  }
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
  case WindowEvent::Kind::Redraw:
    kind = "Redraw";
    break;
  }
  std::printf("[flux] WindowEvent %s handle=%u size=%gx%g dpi=%g\n", kind, e.handle,
              static_cast<double>(e.size.width), static_cast<double>(e.size.height),
              static_cast<double>(e.dpi));
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
  Application* owner_{nullptr};
  std::unique_ptr<EventQueue> eventQueue_;
  std::vector<std::unique_ptr<Window>> ownedWindows_;
  std::vector<Window*> windows_;
  CFRunLoopObserverRef runLoopObserver_{nullptr};

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
        if (e.window) {
          registerWindow(e.window);
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
      if (e.kind == WindowEvent::Kind::Redraw) {
        Window* w = findWindowByHandle(e.handle);
        if (w) {
          Canvas& c = w->canvas();
          c.beginFrame();
          w->render(c);
          c.present();
        }
        return;
      }
      if (e.kind == WindowEvent::Kind::Resize) {
        Window* w = findWindowByHandle(e.handle);
        if (w) {
          Canvas& c = w->canvas();
          c.resize(static_cast<int>(std::lround(static_cast<double>(e.size.width))),
                   static_cast<int>(std::lround(static_cast<double>(e.size.height))));
          c.beginFrame();
          w->render(c);
          c.present();
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
  d->installEventHandlers();

  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

Application::~Application() {
  if (d && d->runLoopObserver_) {
    CFRunLoopRemoveObserver(CFRunLoopGetMain(), d->runLoopObserver_, kCFRunLoopCommonModes);
    CFRelease(d->runLoopObserver_);
    d->runLoopObserver_ = nullptr;
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

int Application::exec() {
  [NSApp activateIgnoringOtherApps:YES];

  d->eventQueue_->dispatch();

  d->runLoopObserver_ = CFRunLoopObserverCreateWithHandler(
      kCFAllocatorDefault, kCFRunLoopBeforeSources, true, 0,
      ^(CFRunLoopObserverRef /*observer*/, CFRunLoopActivity /*activity*/) {
        if (gCurrentApplication && gCurrentApplication->d->eventQueue_) {
          gCurrentApplication->d->eventQueue_->dispatch();
        }
      });
  CFRunLoopAddObserver(CFRunLoopGetMain(), d->runLoopObserver_, kCFRunLoopCommonModes);

  [NSApp run];
  return 0;
}

void Application::quit() { [NSApp stop:nil]; }

Application& Application::instance() {
  if (!gCurrentApplication) {
    throw std::runtime_error("Application not initialized");
  }
  return *gCurrentApplication;
}

} // namespace flux
