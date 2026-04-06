#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace flux {

namespace {

bool envTruthy(char const* v) {
  return v && v[0] != '\0' && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}

bool inputDebugEnabled() {
  static int cached = -1;
  if (cached < 0) {
    cached = envTruthy(std::getenv("FLUX_DEBUG_INPUT")) ? 1 : 0;
  }
  return cached != 0;
}

char const* inputKindName(InputEvent::Kind k) {
  switch (k) {
  case InputEvent::Kind::PointerMove:
    return "PointerMove";
  case InputEvent::Kind::PointerDown:
    return "PointerDown";
  case InputEvent::Kind::PointerUp:
    return "PointerUp";
  case InputEvent::Kind::Scroll:
    return "Scroll";
  case InputEvent::Kind::KeyDown:
    return "KeyDown";
  case InputEvent::Kind::KeyUp:
    return "KeyUp";
  case InputEvent::Kind::TextInput:
    return "TextInput";
  case InputEvent::Kind::TouchBegin:
    return "TouchBegin";
  case InputEvent::Kind::TouchMove:
    return "TouchMove";
  case InputEvent::Kind::TouchEnd:
    return "TouchEnd";
  default:
    return "?";
  }
}

} // namespace

thread_local Runtime* Runtime::sCurrent = nullptr;

Window& Runtime::window() noexcept {
  return window_;
}

Runtime* Runtime::current() noexcept {
  return sCurrent;
}

FocusController& Runtime::focus() noexcept {
  return focus_;
}

HoverController& Runtime::hover() noexcept {
  return hover_;
}

GestureTracker& Runtime::gesture() noexcept {
  return gesture_;
}

LayoutRectCache& Runtime::layoutRects() noexcept {
  return buildOrchestrator_.layoutRects();
}

ActionRegistry& Runtime::actionRegistryForBuild() noexcept {
  return buildOrchestrator_.actionRegistryForBuild();
}

EventMap const& Runtime::mainEventMap() const noexcept {
  return buildOrchestrator_.mainEventMap();
}

void Runtime::requestFocusInSubtree(ComponentKey const& subtreeKey) {
  focus_.requestInSubtree(subtreeKey, mainEventMap());
}

Rect Runtime::buildSlotRect() const {
  return buildOrchestrator_.buildSlotRect();
}

bool Runtime::shuttingDown() const noexcept {
  return shuttingDown_;
}

bool Runtime::imploding() const noexcept {
  return shuttingDown_;
}

Runtime::Runtime(Window& window)
    : window_(window)
    , cursor_(window)
    , buildOrchestrator_(window, focus_, hover_, gesture_)
    , dispatcher_(window, *this, focus_, hover_, gesture_, cursor_, buildOrchestrator_, windowHasFocus_) {
  buildOrchestrator_.subscribeToRebuild([this]() { rebuild(std::nullopt); });
  subscribeInput();
  subscribeWindowEvents();
}

Runtime::~Runtime() {
  shuttingDown_ = true;
}

void Runtime::setRoot(std::unique_ptr<RootHolder> holder) {
  buildOrchestrator_.setRoot(std::move(holder));
  rebuild(std::nullopt);
}

void Runtime::handleInput(InputEvent const& e) {
  dispatcher_.dispatch(e);
}

bool Runtime::isActionCurrentlyEnabled(std::string const& name) const {
  return buildOrchestrator_.actionRegistryCommitted().isHandlerEnabled(focus_.focusedKey(), name,
                                                                       window_.actionDescriptors());
}

std::optional<Rect> Runtime::layoutRectForCurrentComponent() const {
  StateStore* store = StateStore::current();
  if (!store) {
    return std::nullopt;
  }
  return buildOrchestrator_.layoutRects().forCurrentComponent(*store);
}

std::optional<Rect> Runtime::layoutRectForKey(ComponentKey const& key) const {
  return buildOrchestrator_.layoutRects().forKey(key);
}

std::optional<Rect> Runtime::layoutRectForTapAnchor() const {
  return buildOrchestrator_.layoutRects().forTapAnchor(gesture_.pendingTapLeafKey());
}

std::optional<Rect> Runtime::layoutRectForLeafKeyPrefix(ComponentKey const& stableTargetKey) const {
  return buildOrchestrator_.layoutRects().forLeafKeyPrefix(stableTargetKey);
}

std::optional<ComponentKey> Runtime::tapAnchorLeafKeySnapshot() const {
  if (gesture_.pendingTapLeafKey().empty()) {
    return std::nullopt;
  }
  return gesture_.pendingTapLeafKey();
}

void Runtime::rebuild(std::optional<Size> sizeOverride) {
  sCurrent = this;
  buildOrchestrator_.rebuild(sizeOverride, *this);
  sCurrent = nullptr;
}

void Runtime::markMainTreeFullRebuild() noexcept {
  buildOrchestrator_.stateStore().markFullRebuild();
}

void Runtime::onOverlayPushed(OverlayEntry& entry) {
  focus_.onOverlayPushed(entry);
}

void Runtime::onOverlayRemoved(OverlayEntry const& entry) {
  focus_.onOverlayRemoved(entry, shuttingDown_ ? nullptr : &buildOrchestrator_.mainEventMap());
  hover_.onOverlayRemoved(entry.id, shuttingDown_);
}

void Runtime::syncModalOverlayFocusAfterRebuild(OverlayEntry& entry) {
  focus_.syncAfterOverlayRebuild(entry);
}

void Runtime::subscribeInput() {
  if (inputRegistered_) {
    return;
  }
  inputRegistered_ = true;
  unsigned int const hid = window_.handle();
  Application::instance().eventQueue().on<InputEvent>([this, hid](InputEvent const& e) {
    if (e.handle != hid) {
      if (inputDebugEnabled()) {
        std::fprintf(stderr, "[flux:input] dropped event (handle mismatch): event=%u window=%u kind=%s\n",
                     static_cast<unsigned>(e.handle), static_cast<unsigned>(hid), inputKindName(e.kind));
      }
      return;
    }
    handleInput(e);
  });
}

void Runtime::subscribeWindowEvents() {
  unsigned int const hid = window_.handle();
  Application::instance().eventQueue().on<WindowEvent>([this, hid](WindowEvent const& ev) {
    if (ev.handle != hid) {
      return;
    }
    if (ev.kind == WindowEvent::Kind::Resize) {
      rebuild(ev.size);
    } else if (ev.kind == WindowEvent::Kind::FocusLost) {
      windowHasFocus_ = false;
      auto const& entries = window_.overlayManager().entries();
      std::vector<OverlayEntry const*> ovs;
      ovs.reserve(entries.size());
      for (auto const& up : entries) {
        ovs.push_back(up.get());
      }
      gesture_.cancelPress(Point{}, ovs, window_.sceneGraph(), buildOrchestrator_.mainEventMap());
      cursor_.reset();
      hover_.clear();
    } else if (ev.kind == WindowEvent::Kind::FocusGained) {
      windowHasFocus_ = true;
    }
  });
}

} // namespace flux
