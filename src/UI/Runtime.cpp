#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Scene/SceneTreeInteraction.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cstdio>
#include <vector>

#include "UI/DebugFlags.hpp"

namespace flux {

namespace {

bool inputDebugEnabled() {
  return debug::inputEnabled();
}

OverlayEntry const* overlayEntryForScope(OverlayManager const& overlays, std::optional<std::uint64_t> scope) {
  if (!scope.has_value()) {
    return nullptr;
  }
  return overlays.find(OverlayId{*scope});
}

std::optional<Rect> offsetOverlayRect(std::optional<Rect> rect, OverlayEntry const* overlay) {
  if (!rect || !overlay) {
    return rect;
  }
  rect->x += overlay->resolvedFrame.x;
  rect->y += overlay->resolvedFrame.y;
  return rect;
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

ActionRegistry& Runtime::actionRegistryForBuild() noexcept {
  return buildOrchestrator_.actionRegistryForBuild();
}

void Runtime::requestFocusInSubtree(ComponentKey const& subtreeKey, std::optional<OverlayId> overlayScope) {
  if (overlayScope.has_value()) {
    if (OverlayEntry const* entry = window_.overlayManager().find(*overlayScope)) {
      focus_.requestInSubtree(subtreeKey, entry->sceneTree, *overlayScope);
      return;
    }
  }
  focus_.requestInSubtree(subtreeKey, window_.sceneTree());
}

Rect Runtime::buildSlotRect() const {
  return buildOrchestrator_.buildSlotRect();
}

bool Runtime::shuttingDown() const noexcept {
  return shuttingDown_;
}

void Runtime::beginShutdown() noexcept {
  shuttingDown_ = true;
}

bool Runtime::imploding() const noexcept {
  return shuttingDown_;
}

Runtime::Runtime(Window& window)
    : window_(window)
    , cursor_(window)
    , buildOrchestrator_(window, focus_, hover_, gesture_)
    , dispatcher_(window, *this, focus_, hover_, gesture_, cursor_, buildOrchestrator_, windowHasFocus_) {
  auto dirtyMarker = [this](ComponentKey const& key, std::optional<OverlayId> overlayScope) {
    return markInteractiveDirty(key, overlayScope);
  };
  focus_.setDirtyMarker(dirtyMarker);
  hover_.setDirtyMarker(dirtyMarker);
  gesture_.setDirtyMarker(dirtyMarker);
  buildOrchestrator_.subscribeToRebuild([this]() { rebuild(std::nullopt); });
  subscribeInput();
  subscribeWindowEvents();
}

Runtime::~Runtime() {
  beginShutdown();
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

bool Runtime::wantsTextInput() const {
  if (!windowHasFocus_ || focus_.focusedKey().empty()) {
    return false;
  }

  InteractionData const* interaction = nullptr;
  if (std::optional<OverlayId> overlayScope = focus_.focusInOverlay()) {
    if (OverlayEntry const* entry = window_.overlayManager().find(*overlayScope)) {
      interaction = findInteractionByKey(entry->sceneTree, focus_.focusedKey()).second;
    }
  } else if (window_.hasSceneTree()) {
    interaction = findInteractionByKey(window_.sceneTree(), focus_.focusedKey()).second;
  }

  return interaction && static_cast<bool>(interaction->onTextInput);
}

std::optional<Rect> Runtime::layoutRectForCurrentComponent() const {
  StateStore* store = StateStore::current();
  if (!store) {
    return std::nullopt;
  }
  if (OverlayEntry const* overlay = overlayEntryForScope(window_.overlayManager(), store->overlayScope())) {
    return offsetOverlayRect(overlay->sceneGeometry.forCurrentComponent(*store), overlay);
  }
  return buildOrchestrator_.sceneGeometry().forCurrentComponent(*store);
}

std::optional<Rect> Runtime::layoutRectForKey(ComponentKey const& key) const {
  StateStore* store = StateStore::current();
  if (store) {
    if (OverlayEntry const* overlay = overlayEntryForScope(window_.overlayManager(), store->overlayScope())) {
      return offsetOverlayRect(overlay->sceneGeometry.forKey(key), overlay);
    }
  }
  return buildOrchestrator_.sceneGeometry().forKey(key);
}

std::optional<Rect> Runtime::layoutRectForTapAnchor() const {
  if (gesture_.pendingTapLeafKey().empty()) {
    return std::nullopt;
  }
  if (std::optional<OverlayId> overlayScope = gesture_.pendingTapOverlayScope()) {
    if (OverlayEntry const* overlay = window_.overlayManager().find(*overlayScope)) {
      return offsetOverlayRect(overlay->sceneGeometry.forTapAnchor(gesture_.pendingTapLeafKey()), overlay);
    }
  }
  return buildOrchestrator_.sceneGeometry().forTapAnchor(gesture_.pendingTapLeafKey());
}

std::optional<Rect> Runtime::layoutRectForLeafKeyPrefix(ComponentKey const& stableTargetKey) const {
  StateStore* store = StateStore::current();
  if (store) {
    if (OverlayEntry const* overlay = overlayEntryForScope(window_.overlayManager(), store->overlayScope())) {
      return offsetOverlayRect(overlay->sceneGeometry.forLeafKeyPrefix(stableTargetKey), overlay);
    }
  }
  return buildOrchestrator_.sceneGeometry().forLeafKeyPrefix(stableTargetKey);
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

bool Runtime::markInteractiveDirty(ComponentKey const& key, std::optional<OverlayId> overlayScope) {
  if (key.empty()) {
    if (Application::hasInstance()) {
      Application::instance().markReactiveDirty();
    }
    return false;
  }
  if (overlayScope.has_value()) {
    if (OverlayEntry* entry = window_.overlayManager().find(*overlayScope);
        entry && entry->stateStore) {
      entry->stateStore->markCompositeDirty(key);
      return true;
    }
    if (Application::hasInstance()) {
      Application::instance().markReactiveDirty();
    }
    return false;
  }
  buildOrchestrator_.stateStore().markCompositeDirty(key);
  return true;
}

void Runtime::onOverlayPushed(OverlayEntry& entry) {
  focus_.onOverlayPushed(entry);
}

void Runtime::onOverlayRemoved(OverlayEntry const& entry) {
  focus_.onOverlayRemoved(entry, shuttingDown_ ? nullptr : &window_.sceneTree());
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
      gesture_.cancelPress(Point{}, ovs, window_.sceneTree());
      cursor_.reset();
      hover_.clear();
    } else if (ev.kind == WindowEvent::Kind::FocusGained) {
      windowHasFocus_ = true;
    }
  });
}

} // namespace flux
