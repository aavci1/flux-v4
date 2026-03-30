#include <Flux/Detail/Runtime.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/SceneGraphBounds.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Scene/HitTester.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/EventMap.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace flux {

bool Runtime::isActionCurrentlyEnabled(std::string const& name) const {
  return actionRegistryCommitted_.isHandlerEnabled(focusedKey_, name, window_.actionDescriptors());
}

void Runtime::setRoot(std::unique_ptr<RootHolder> holder) {
  rootHolder_ = std::move(holder);
  rebuild();
}

namespace {

/// Whole-point layout size avoids subpixel oscillation in `NSView.bounds` during live resize.
Size snapRootLayoutSize(Size s) {
  return {std::max(1.f, std::round(s.width)), std::max(1.f, std::round(s.height))};
}

/// Logical points; matches UIKit default touch slop (roadmap §2).
constexpr float kTapSlop = 8.f;

// Set FLUX_DEBUG_INPUT=1 to log input routing. FLUX_DEBUG_INPUT_VERBOSE=1 also logs every move.
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

bool inputDebugVerbose() {
  static int cached = -1;
  if (cached < 0) {
    cached = envTruthy(std::getenv("FLUX_DEBUG_INPUT_VERBOSE")) ? 1 : 0;
  }
  return cached != 0;
}

void logNodeId(char const* label, NodeId id) {
  if (!id.isValid()) {
    std::fprintf(stderr, "[flux:input] %s: (invalid id)\n", label);
  } else {
    std::fprintf(stderr, "[flux:input] %s: index=%u generation=%u\n", label, id.index, id.generation);
  }
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

/// Prefer targets that are not `cursorPassthrough` (e.g. ScrollView's full-viewport drag layer) so
/// hits reach stacked content (buttons); fall back to include passthrough so empty viewport drags work.
std::optional<HitResult> hitTestPointerTarget(EventMap const& em, SceneGraph const& graph, Point windowPoint) {
  auto const acceptAll = [&em](NodeId id) { return em.find(id) != nullptr; };
  auto const acceptPrimary = [&em](NodeId id) {
    EventHandlers const* h = em.find(id);
    if (!h) {
      return false;
    }
    if (h->cursorPassthrough) {
      return false;
    }
    return true;
  };
  HitTester tester{};
  if (auto r = tester.hitTest(graph, windowPoint, acceptPrimary)) {
    return r;
  }
  return tester.hitTest(graph, windowPoint, acceptAll);
}

} // namespace

thread_local Runtime* Runtime::sCurrent = nullptr;

namespace {

bool shouldClaimFocus(EventHandlers const& h) { return h.focusable; }

} // namespace

bool Runtime::isFocusInSubtree(ComponentKey const& key) const noexcept {
  if (focusedKey_.empty() || key.size() > focusedKey_.size()) {
    return false;
  }
  if (!std::equal(key.begin(), key.end(), focusedKey_.begin())) {
    return false;
  }
  StateStore* store = StateStore::current();
  if (!store) {
    return false;
  }
  std::optional<std::uint64_t> const os = store->overlayScope();
  if (os.has_value()) {
    return focusInOverlay_.has_value() && focusInOverlay_->value == *os;
  }
  return !focusInOverlay_.has_value();
}

bool Runtime::isHoverInSubtree(ComponentKey const& key) const noexcept {
  if (hoveredKey_.empty() || key.size() > hoveredKey_.size()) {
    return false;
  }
  if (!std::equal(key.begin(), key.end(), hoveredKey_.begin())) {
    return false;
  }
  StateStore* store = StateStore::current();
  if (!store) {
    return false;
  }
  std::optional<std::uint64_t> const os = store->overlayScope();
  if (os.has_value()) {
    return hoverInOverlay_.has_value() && hoverInOverlay_->value == *os;
  }
  return !hoverInOverlay_.has_value();
}

void Runtime::requestFocusInSubtree(ComponentKey const& subtreeKey) {
  if (subtreeKey.empty()) {
    return;
  }
  auto const& order = eventMap_.focusOrder();
  for (ComponentKey const& leafKey : order) {
    if (leafKey.size() >= subtreeKey.size() &&
        std::equal(subtreeKey.begin(), subtreeKey.end(), leafKey.begin())) {
      setFocus(leafKey, std::nullopt, FocusInputKind::Keyboard);
      return;
    }
  }
}

void Runtime::setFocus(ComponentKey const& key, std::optional<OverlayId> overlayScope, FocusInputKind kind) {
  if (focusedKey_ == key && focusInOverlay_ == overlayScope) {
    return;
  }
  focusedKey_ = key;
  focusInOverlay_ = overlayScope;
  lastFocusInputKind_ = kind;
  Application::instance().markReactiveDirty();
}

void Runtime::clearFocus() {
  if (focusedKey_.empty() && !focusInOverlay_.has_value()) {
    return;
  }
  focusedKey_.clear();
  focusInOverlay_.reset();
  Application::instance().markReactiveDirty();
}

void Runtime::setHovered(ComponentKey const& key, std::optional<OverlayId> overlayScope) {
  if (hoveredKey_ == key && hoverInOverlay_ == overlayScope) {
    return;
  }
  hoveredKey_ = key;
  hoverInOverlay_ = overlayScope;
  Application::instance().markReactiveDirty();
}

void Runtime::clearHovered() {
  if (hoveredKey_.empty() && !hoverInOverlay_.has_value()) {
    return;
  }
  hoveredKey_.clear();
  hoverInOverlay_.reset();
  Application::instance().markReactiveDirty();
}

void Runtime::cycleTabFocusInMap(EventMap const& em, bool reverse, std::optional<OverlayId> overlayId) {
  auto const& order = em.focusOrder();
  if (order.empty()) {
    return;
  }

  if (focusedKey_.empty()) {
    setFocus(reverse ? order.back() : order.front(), overlayId, FocusInputKind::Keyboard);
    return;
  }

  auto it = std::find(order.begin(), order.end(), focusedKey_);
  if (it == order.end()) {
    setFocus(reverse ? order.back() : order.front(), overlayId, FocusInputKind::Keyboard);
    return;
  }

  if (!reverse) {
    ++it;
    setFocus(it == order.end() ? order.front() : *it, overlayId, FocusInputKind::Keyboard);
  } else {
    setFocus(it == order.begin() ? order.back() : *std::prev(it), overlayId, FocusInputKind::Keyboard);
  }
}

void Runtime::cycleTabFocusNonModal(bool reverse) {
  std::vector<std::pair<ComponentKey, std::optional<OverlayId>>> merged;
  for (std::unique_ptr<OverlayEntry> const& up : window_.overlayManager().entries()) {
    OverlayEntry const& e = *up;
    for (ComponentKey const& k : e.eventMap.focusOrder()) {
      merged.push_back({k, e.id});
    }
  }
  for (ComponentKey const& k : eventMap_.focusOrder()) {
    merged.push_back({k, std::nullopt});
  }
  if (merged.empty()) {
    return;
  }

  std::size_t idx = 0;
  bool found = false;
  for (std::size_t i = 0; i < merged.size(); ++i) {
    if (merged[i].first == focusedKey_ && focusInOverlay_ == merged[i].second) {
      idx = i;
      found = true;
      break;
    }
  }

  if (!found || focusedKey_.empty()) {
    if (reverse) {
      setFocus(merged.back().first, merged.back().second, FocusInputKind::Keyboard);
    } else {
      setFocus(merged.front().first, merged.front().second, FocusInputKind::Keyboard);
    }
    return;
  }

  if (!reverse) {
    std::size_t const next = (idx + 1) % merged.size();
    setFocus(merged[next].first, merged[next].second, FocusInputKind::Keyboard);
  } else {
    std::size_t const prev = idx == 0 ? merged.size() - 1 : idx - 1;
    setFocus(merged[prev].first, merged[prev].second, FocusInputKind::Keyboard);
  }
}

void Runtime::fillLayoutRectCache(SceneGraph const& graph, BuildContext const& ctx) {
  layoutRectPrev_.swap(layoutRectCurrent_);
  layoutRectCurrent_.clear();
  for (auto const& [key, nodeId] : ctx.subtreeRootLayers()) {
    Mat3 const pw = subtreeAncestorWorldTransform(graph, nodeId);
    layoutRectCurrent_[key] = unionSubtreeBounds(graph, nodeId, pw);
  }
}

std::optional<Rect> Runtime::layoutRectForCurrentComponent() const {
  StateStore* store = StateStore::current();
  if (!store) {
    return std::nullopt;
  }
  // Read layoutRectCurrent_, not layoutRectPrev_. After fillLayoutRectCache swaps maps, `prev` holds
  // the map that was current before the swap — one frame older than `current` at build time. During
  // body() we need the last completed layout (what `current` still holds until this rebuild's fill runs).
  auto it = layoutRectCurrent_.find(store->currentComponentKey());
  if (it == layoutRectCurrent_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<Rect> Runtime::layoutRectForKey(ComponentKey const& key) const {
  auto it = layoutRectCurrent_.find(key);
  if (it == layoutRectCurrent_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<Rect> Runtime::layoutRectForTapLeafKey(ComponentKey const& stableTargetKey) const {
  for (std::size_t len = stableTargetKey.size(); len > 0; --len) {
    ComponentKey prefix(stableTargetKey.begin(),
                        stableTargetKey.begin() + static_cast<std::ptrdiff_t>(len));
    if (auto it = layoutRectCurrent_.find(prefix); it != layoutRectCurrent_.end()) {
      return it->second;
    }
  }
  return std::nullopt;
}

std::optional<Rect> Runtime::layoutRectForTapAnchor() const {
  if (pendingTapTargetKey_.empty()) {
    return std::nullopt;
  }
  return layoutRectForTapLeafKey(pendingTapTargetKey_);
}

std::optional<Rect> Runtime::layoutRectForLeafKeyPrefix(ComponentKey const& stableTargetKey) const {
  return layoutRectForTapLeafKey(stableTargetKey);
}

std::optional<ComponentKey> Runtime::tapAnchorLeafKeySnapshot() const {
  if (pendingTapTargetKey_.empty()) {
    return std::nullopt;
  }
  return pendingTapTargetKey_;
}

void Runtime::onOverlayPushed(OverlayEntry& entry) {
  if (!entry.config.modal) {
    return;
  }
  entry.preFocusKey = focusedKey_;
  focusInOverlay_ = entry.id;
  focusedKey_.clear();
}

void Runtime::onOverlayRemoved(OverlayEntry const& entry) {
  if (imploding_) {
    if (focusInOverlay_ == entry.id) {
      focusInOverlay_.reset();
    }
    if (hoverInOverlay_ == entry.id) {
      hoverInOverlay_.reset();
    }
    return;
  }
  if (entry.config.modal) {
    if (focusInOverlay_ == entry.id) {
      focusInOverlay_.reset();
      focusedKey_ = entry.preFocusKey;
      lastFocusInputKind_ = FocusInputKind::Keyboard;
      if (!focusedKey_.empty()) {
        auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
        (void)id;
        if (!h) {
          focusedKey_.clear();
        }
      }
    }
  }
  if (hoverInOverlay_ == entry.id) {
    hoverInOverlay_.reset();
    hoveredKey_.clear();
  }
}

void Runtime::syncModalOverlayFocusAfterRebuild(OverlayEntry& entry) {
  if (!entry.config.modal) {
    return;
  }
  if (focusInOverlay_ != entry.id) {
    return;
  }
  auto const& order = entry.eventMap.focusOrder();
  if (order.empty()) {
    return;
  }
  auto const [id, h] = entry.eventMap.findWithIdByKey(focusedKey_);
  (void)id;
  if (!h) {
    focusedKey_ = order.front();
    lastFocusInputKind_ = FocusInputKind::Keyboard;
  }
}

bool Runtime::pressKeyMatchesStoreContext(StateStore const& store) const {
  if (!activePress_) {
    return false;
  }
  std::optional<std::uint64_t> const oscope = store.overlayScope();
  if (activePress_->overlayScope.has_value()) {
    return oscope.has_value() && *oscope == activePress_->overlayScope->value;
  }
  return !oscope.has_value();
}

Runtime::Runtime(Window& window) : window_(window) {
  subscribeToRebuild();
  subscribeInput();
  subscribeWindowEvents();
}

Runtime::~Runtime() {
  if (rebuildHandle_.isValid()) {
    Application::instance().unobserveNextFrame(rebuildHandle_);
  }
  imploding_ = true;
}

void Runtime::rebuild(std::optional<Size> sizeOverride) {
  // Only drop an active gesture when the window size changes; rebuilds from reactive/layout
  // animation reuse new NodeIds and would otherwise break PointerUp matching for taps.
  if (sizeOverride.has_value()) {
    activePress_ = std::nullopt;
  }
  sCurrent = this;
  SceneGraph& graph = window_.sceneGraph();
  graph.clear();

  layoutEngine_.resetForBuild();

  actionRegistryBuild_.beginRebuild();

  stateStore_.beginRebuild();
  StateStore::setCurrent(&stateStore_);

  EventMap newMap;
  BuildContext ctx{graph, newMap, Application::instance().textSystem(), layoutEngine_};
  Size const raw = sizeOverride.value_or(window_.getSize());
  Size const sz = snapRootLayoutSize(raw);
  LayoutConstraints rootCs{};
  rootCs.maxWidth = sz.width;
  rootCs.maxHeight = sz.height;
  ctx.pushConstraints(rootCs);
  if (rootHolder_) {
    rootHolder_->buildInto(ctx);
  }
  ctx.popConstraints();

  fillLayoutRectCache(graph, ctx);

  StateStore::setCurrent(nullptr);
  stateStore_.endRebuild();

  eventMap_ = std::move(newMap);
  if (!focusInOverlay_) {
    if (!focusedKey_.empty()) {
      auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
      (void)id;
      if (!h) {
        focusedKey_.clear();
      }
    }
  }

  window_.overlayManager().rebuild(sz, *this);

  std::swap(actionRegistryBuild_, actionRegistryCommitted_);

  if (inputDebugEnabled()) {
    std::fprintf(stderr,
                 "[flux:input] rebuild layout=%.1fx%.1f scene root children (if any) updated\n",
                 static_cast<double>(sz.width), static_cast<double>(sz.height));
  }
  window_.requestRedraw();
  sCurrent = nullptr;
}

void Runtime::subscribeToRebuild() {
  rebuildHandle_ = Application::instance().onNextFrameNeeded([this] { rebuild(); });
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
      // Use the size captured when the event was posted; re-querying the view during dispatch can
      // differ slightly from bounds updates and cause visible jitter during live resize.
      rebuild(ev.size);
    } else if (ev.kind == WindowEvent::Kind::FocusLost) {
      windowHasFocus_ = false;
      cancelActivePress(Point{});
      currentCursor_ = Cursor::Arrow;
      clearHovered();
    } else if (ev.kind == WindowEvent::Kind::FocusGained) {
      windowHasFocus_ = true;
    }
  });
}

std::pair<NodeId, EventHandlers const*> Runtime::findPressHandlersWithNode(PressState const& ps) const {
  if (ps.overlayScope.has_value()) {
    for (std::unique_ptr<OverlayEntry> const& up : window_.overlayManager().entries()) {
      OverlayEntry const& e = *up;
      if (e.id.value != ps.overlayScope->value) {
        continue;
      }
      if (EventHandlers const* h = e.eventMap.find(ps.nodeId)) {
        return {ps.nodeId, h};
      }
      if (!ps.stableTargetKey.empty()) {
        return e.eventMap.findWithIdByKey(ps.stableTargetKey);
      }
      return {kInvalidNodeId, nullptr};
    }
    return {kInvalidNodeId, nullptr};
  }
  if (EventHandlers const* h = eventMap_.find(ps.nodeId)) {
    return {ps.nodeId, h};
  }
  if (!ps.stableTargetKey.empty()) {
    return eventMap_.findWithIdByKey(ps.stableTargetKey);
  }
  return {kInvalidNodeId, nullptr};
}

SceneGraph const& Runtime::sceneGraphForPress(PressState const& ps) const {
  if (ps.overlayScope.has_value()) {
    for (std::unique_ptr<OverlayEntry> const& up : window_.overlayManager().entries()) {
      OverlayEntry const& e = *up;
      if (e.id.value == ps.overlayScope->value) {
        return e.graph;
      }
    }
  }
  return window_.sceneGraph();
}

void Runtime::cancelActivePress(Point windowPoint) {
  if (!activePress_) {
    return;
  }
  SceneGraph const& graph = sceneGraphForPress(*activePress_);
  auto const [currentId, h] = findPressHandlersWithNode(*activePress_);
  if (h && h->onPointerUp && currentId.isValid()) {
    HitTester tester{};
    std::optional<Point> local = tester.localPointForNode(graph, activePress_->downPoint, currentId);
    if (!local) {
      local = tester.localPointForNode(graph, windowPoint, currentId);
    }
    h->onPointerUp(local.value_or(Point{0.f, 0.f}));
  }
  activePress_ = std::nullopt;
  Application::instance().markReactiveDirty();
}

void Runtime::applyCursor(Cursor kind) {
  if (kind == currentCursor_) {
    return;
  }
  currentCursor_ = kind;
  window_.setCursor(kind);
}

void Runtime::updateCursorForPoint(Point windowPoint) {
  if (activePress_) {
    auto const [pressId, pressed] = findPressHandlersWithNode(*activePress_);
    (void)pressId;
    if (pressed && pressed->cursor != Cursor::Inherit) {
      applyCursor(pressed->cursor);
      return;
    }
  }

  std::vector<std::unique_ptr<OverlayEntry>> const& oentries = window_.overlayManager().entries();
  for (auto it = oentries.rbegin(); it != oentries.rend(); ++it) {
    OverlayEntry const& oe = **it;
    Point const local{windowPoint.x - oe.resolvedFrame.x, windowPoint.y - oe.resolvedFrame.y};
    auto const acceptFn = [&oe](NodeId id) {
      EventHandlers const* h = oe.eventMap.find(id);
      if (!h) {
        return false;
      }
      if (h->cursorPassthrough) {
        return false;
      }
      if (h->cursor == Cursor::Inherit) {
        return false;
      }
      return true;
    };
    HitTester tester{};
    if (auto hit = tester.hitTest(oe.graph, local, acceptFn)) {
      if (EventHandlers const* h = oe.eventMap.find(hit->nodeId)) {
        applyCursor(h->cursor);
        return;
      }
    }
  }

  SceneGraph const& graph = window_.sceneGraph();
  auto const acceptFn = [this](NodeId id) {
    EventHandlers const* h = eventMap_.find(id);
    if (!h) {
      return false;
    }
    if (h->cursorPassthrough) {
      return false;
    }
    if (h->cursor == Cursor::Inherit) {
      return false;
    }
    return true;
  };
  auto hit = HitTester{}.hitTest(graph, windowPoint, acceptFn);
  if (hit) {
    if (EventHandlers const* h = eventMap_.find(hit->nodeId)) {
      applyCursor(h->cursor);
    } else {
      applyCursor(Cursor::Arrow);
    }
  } else {
    applyCursor(Cursor::Arrow);
  }
}

void Runtime::updateHoveredForPoint(Point windowPoint) {
  std::vector<std::unique_ptr<OverlayEntry>> const& oentries = window_.overlayManager().entries();
  for (auto it = oentries.rbegin(); it != oentries.rend(); ++it) {
    OverlayEntry const& oe = **it;
    Point const local{windowPoint.x - oe.resolvedFrame.x, windowPoint.y - oe.resolvedFrame.y};
    auto const acceptFn = [&oe](NodeId id) -> bool {
      EventHandlers const* h = oe.eventMap.find(id);
      return h && !h->cursorPassthrough;
    };
    HitTester tester{};
    if (auto hit = tester.hitTest(oe.graph, local, acceptFn)) {
      if (EventHandlers const* h = oe.eventMap.find(hit->nodeId)) {
        setHovered(h->stableTargetKey, oe.id);
        return;
      }
    }
  }

  SceneGraph const& graph = window_.sceneGraph();
  auto const acceptFn = [this](NodeId id) -> bool {
    EventHandlers const* h = eventMap_.find(id);
    if (!h) {
      return false;
    }
    if (h->cursorPassthrough) {
      return false;
    }
    return true;
  };

  auto hit = HitTester{}.hitTest(graph, windowPoint, acceptFn);
  if (hit) {
    if (EventHandlers const* h = eventMap_.find(hit->nodeId)) {
      setHovered(h->stableTargetKey, std::nullopt);
    } else {
      clearHovered();
    }
  } else {
    clearHovered();
  }
}

void Runtime::handleInput(InputEvent const& e) {
  SceneGraph const& graph = window_.sceneGraph();
  bool const dbg = inputDebugEnabled();
  bool const dbgMove = dbg && inputDebugVerbose();

  if (e.kind == InputEvent::Kind::KeyDown) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] KeyDown key=%u modifiers=%u\n", static_cast<unsigned>(e.key),
                   static_cast<unsigned>(e.modifiers));
    }
    if (window_.overlayManager().hasOverlays()) {
      OverlayEntry const* top = window_.overlayManager().top();
      if (top->config.modal) {
        if (e.key == keys::Escape && top->config.dismissOnEscape) {
          window_.removeOverlay(top->id);
          return;
        }
        if (e.key == keys::Tab && windowHasFocus_) {
          bool const reverse = any(e.modifiers & Modifiers::Shift);
          cycleTabFocusInMap(top->eventMap, reverse, top->id);
          return;
        }
        if (!focusedKey_.empty() && windowHasFocus_) {
          auto const [id, h] = top->eventMap.findWithIdByKey(focusedKey_);
          (void)id;
          if (h && h->onKeyDown) {
            h->onKeyDown(e.key, e.modifiers);
          }
        }
        return;
      }
      if (e.key == keys::Escape && top->config.dismissOnEscape) {
        window_.removeOverlay(top->id);
        return;
      }
      if (e.key == keys::Tab && windowHasFocus_) {
        bool const reverse = any(e.modifiers & Modifiers::Shift);
        cycleTabFocusNonModal(reverse);
        return;
      }
    }
    if (e.key == keys::Tab && windowHasFocus_) {
      bool const reverse = any(e.modifiers & Modifiers::Shift);
      cycleTabFocusInMap(eventMap_, reverse, std::nullopt);
      return;
    }
    if (windowHasFocus_) {
      if (actionRegistryCommitted_.dispatchShortcut(focusedKey_, e.key, e.modifiers, window_.actionDescriptors())) {
        return;
      }
    }
    if (!focusedKey_.empty() && windowHasFocus_) {
      auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
      (void)id;
      if (h && h->onKeyDown) {
        h->onKeyDown(e.key, e.modifiers);
      }
    }
    return;
  }
  if (e.kind == InputEvent::Kind::KeyUp) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] KeyUp key=%u modifiers=%u\n", static_cast<unsigned>(e.key),
                   static_cast<unsigned>(e.modifiers));
    }
    if (window_.overlayManager().hasOverlays()) {
      OverlayEntry const* top = window_.overlayManager().top();
      if (top->config.modal) {
        if (!focusedKey_.empty() && windowHasFocus_) {
          auto const [id, h] = top->eventMap.findWithIdByKey(focusedKey_);
          (void)id;
          if (h && h->onKeyUp) {
            h->onKeyUp(e.key, e.modifiers);
          }
        }
        return;
      }
    }
    if (!focusedKey_.empty() && windowHasFocus_) {
      auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
      (void)id;
      if (h && h->onKeyUp) {
        h->onKeyUp(e.key, e.modifiers);
      }
    }
    return;
  }
  if (e.kind == InputEvent::Kind::TextInput) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] TextInput len=%zu\n", e.text.size());
    }
    if (window_.overlayManager().hasOverlays()) {
      OverlayEntry const* top = window_.overlayManager().top();
      if (top->config.modal) {
        if (!focusedKey_.empty() && windowHasFocus_) {
          auto const [id, h] = top->eventMap.findWithIdByKey(focusedKey_);
          (void)id;
          if (h && h->onTextInput) {
            h->onTextInput(e.text);
          }
        }
        return;
      }
    }
    if (!focusedKey_.empty() && windowHasFocus_) {
      auto const [id, h] = eventMap_.findWithIdByKey(focusedKey_);
      (void)id;
      if (h && h->onTextInput) {
        h->onTextInput(e.text);
      }
    }
    return;
  }

  if (e.kind == InputEvent::Kind::Scroll) {
    Point const p{e.position.x, e.position.y};
    Vec2 delta = e.scrollDelta;
    if (!e.preciseScrollDelta) {
      constexpr float kLineHeight = 40.f;
      delta.x *= kLineHeight;
      delta.y *= kLineHeight;
    }
    HitTester tester{};
    std::vector<std::unique_ptr<OverlayEntry>> const& oentries = window_.overlayManager().entries();
    for (auto it = oentries.rbegin(); it != oentries.rend(); ++it) {
      OverlayEntry const& oe = **it;
      Point const pl{p.x - oe.resolvedFrame.x, p.y - oe.resolvedFrame.y};
      auto const acceptScroll = [&oe](NodeId id) {
        EventHandlers const* h = oe.eventMap.find(id);
        return h && h->onScroll;
      };
      if (auto hit = tester.hitTest(oe.graph, pl, acceptScroll)) {
        if (EventHandlers const* h = oe.eventMap.find(hit->nodeId); h && h->onScroll) {
          h->onScroll(delta);
        }
        return;
      }
    }
    auto const acceptScroll = [this](NodeId id) {
      EventHandlers const* h = eventMap_.find(id);
      return h && h->onScroll;
    };
    if (dbg) {
      if (auto front = tester.hitTest(graph, p)) {
        std::fprintf(stderr,
                     "[flux:input] Scroll pos=(%.1f,%.1f) delta=(%.2f,%.2f) frontmost geom: local=(%.1f,%.1f) ",
                     static_cast<double>(p.x), static_cast<double>(p.y),
                     static_cast<double>(delta.x), static_cast<double>(delta.y),
                     static_cast<double>(front->localPoint.x), static_cast<double>(front->localPoint.y));
        logNodeId("frontmost (any)", front->nodeId);
      } else {
        std::fprintf(stderr,
                     "[flux:input] Scroll pos=(%.1f,%.1f) delta=(%.2f,%.2f) frontmost geom: (no hit)\n",
                     static_cast<double>(p.x), static_cast<double>(p.y),
                     static_cast<double>(delta.x), static_cast<double>(delta.y));
      }
    }
    if (auto hit = tester.hitTest(graph, p, acceptScroll)) {
      if (dbg) {
        std::fprintf(stderr,
                     "[flux:input] Scroll filtered (has onScroll): local=(%.1f,%.1f) ",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
        logNodeId("scroll target", hit->nodeId);
      }
      if (EventHandlers const* h = eventMap_.find(hit->nodeId); h && h->onScroll) {
        h->onScroll(delta);
        if (dbg) {
          std::fprintf(stderr, "[flux:input] onScroll invoked\n");
        }
      }
    } else if (dbg) {
      std::fprintf(stderr, "[flux:input] Scroll: no node with onScroll under cursor\n");
    }
    return;
  }

  if (e.kind != InputEvent::Kind::PointerDown && e.kind != InputEvent::Kind::PointerUp &&
      e.kind != InputEvent::Kind::PointerMove) {
    return;
  }
  Point const p{e.position.x, e.position.y};

  static int moveLogCounter = 0;

  if (e.kind == InputEvent::Kind::PointerDown) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerDown pos=(%.1f,%.1f)\n", static_cast<double>(p.x),
                   static_cast<double>(p.y));
    }
    activePress_ = std::nullopt;
    std::vector<std::unique_ptr<OverlayEntry>> const& oentries = window_.overlayManager().entries();
    for (auto it = oentries.rbegin(); it != oentries.rend(); ++it) {
      OverlayEntry const& oe = **it;
      Point const pl{p.x - oe.resolvedFrame.x, p.y - oe.resolvedFrame.y};
      if (auto hit = hitTestPointerTarget(oe.eventMap, oe.graph, pl)) {
        if (dbg) {
          std::fprintf(stderr,
                       "[flux:input] PointerDown hit local=(%.1f,%.1f) ",
                       static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
          logNodeId("press target", hit->nodeId);
        }
        if (EventHandlers const* h = oe.eventMap.find(hit->nodeId)) {
          PressState ps{};
          ps.nodeId = hit->nodeId;
          ps.stableTargetKey = h->stableTargetKey;
          ps.downPoint = p;
          ps.cancelled = false;
          ps.hadOnTapOnDown = static_cast<bool>(h->onTap);
          ps.overlayScope = oe.id;
          activePress_ = std::move(ps);
          Application::instance().markReactiveDirty();
          if (h->onPointerDown) {
            h->onPointerDown(hit->localPoint);
          }
          if (oe.config.modal && shouldClaimFocus(*h)) {
            setFocus(h->stableTargetKey, oe.id, FocusInputKind::Pointer);
          }
        }
        updateCursorForPoint(p);
        updateHoveredForPoint(p);
        return;
      }
      if (oe.config.modal) {
        updateCursorForPoint(p);
        updateHoveredForPoint(p);
        return;
      }
    }
    // Pointer missed every overlay graph (non-modal overlays fell through). Dismiss top overlay when
    // configured — same as tapping outside the card (transparent backdrop has no capture layer).
    if (window_.overlayManager().hasOverlays()) {
      OverlayEntry const* top = window_.overlayManager().top();
      if (top && !top->config.modal && top->config.dismissOnOutsideTap) {
        window_.removeOverlay(top->id);
      }
    }
    auto hit = hitTestPointerTarget(eventMap_, graph, p);
    if (hit) {
      if (dbg) {
        std::fprintf(stderr,
                     "[flux:input] PointerDown hit local=(%.1f,%.1f) ",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
        logNodeId("press target", hit->nodeId);
      }
      if (EventHandlers const* h = eventMap_.find(hit->nodeId)) {
        PressState ps{};
        ps.nodeId = hit->nodeId;
        ps.stableTargetKey = h->stableTargetKey;
        ps.downPoint = p;
        ps.cancelled = false;
        ps.hadOnTapOnDown = static_cast<bool>(h->onTap);
        activePress_ = std::move(ps);
        Application::instance().markReactiveDirty();
        if (h->onPointerDown) {
          h->onPointerDown(hit->localPoint);
        }
        if (shouldClaimFocus(*h)) {
          setFocus(h->stableTargetKey, std::nullopt, FocusInputKind::Pointer);
        }
      }
    } else if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerDown: no interactive node under cursor\n");
    }
    updateCursorForPoint(p);
    updateHoveredForPoint(p);
    return;
  }

  if (e.kind == InputEvent::Kind::PointerMove) {
    if (activePress_ && !(e.pressedButtons & 1)) {
      cancelActivePress(p);
    }
    bool const logThisMove = dbgMove || (dbg && (++moveLogCounter % 15 == 0));
    if (logThisMove) {
      std::fprintf(stderr, "[flux:input] PointerMove pos=(%.1f,%.1f) activePress=%s\n",
                   static_cast<double>(p.x), static_cast<double>(p.y),
                   activePress_ ? "yes" : "no");
      if (activePress_) {
        logNodeId("  active node", activePress_->nodeId);
      }
    }
    if (activePress_) {
      float const dx = p.x - activePress_->downPoint.x;
      float const dy = p.y - activePress_->downPoint.y;
      if (dx * dx + dy * dy > kTapSlop * kTapSlop) {
        activePress_->cancelled = true;
      }
    }
    HitTester tester{};
    if (activePress_) {
      auto const [pressId, pressed] = findPressHandlersWithNode(*activePress_);
      if (pressed && pressed->onPointerMove && pressId.isValid()) {
        SceneGraph const& gPress = sceneGraphForPress(*activePress_);
        if (auto local = tester.localPointForNode(gPress, p, pressId)) {
          if (logThisMove) {
            std::fprintf(stderr,
                         "[flux:input] PointerMove routed to press target local=(%.1f,%.1f)\n",
                         static_cast<double>(local->x), static_cast<double>(local->y));
          }
          pressed->onPointerMove(*local);
          updateCursorForPoint(p);
          updateHoveredForPoint(p);
          return;
        }
        if (dbg) {
          std::fprintf(stderr,
                       "[flux:input] PointerMove: localPointForNode FAILED for active press (bad graph?)\n");
        }
      }
    }
    std::vector<std::unique_ptr<OverlayEntry>> const& oentries = window_.overlayManager().entries();
    for (auto oit = oentries.rbegin(); oit != oentries.rend(); ++oit) {
      OverlayEntry const& oe = **oit;
      Point const pl{p.x - oe.resolvedFrame.x, p.y - oe.resolvedFrame.y};
      if (auto hit = hitTestPointerTarget(oe.eventMap, oe.graph, pl)) {
        if (logThisMove) {
          std::fprintf(stderr,
                       "[flux:input] PointerMove hit-test path local=(%.1f,%.1f) ",
                       static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
          logNodeId("under cursor", hit->nodeId);
        }
        if (EventHandlers const* h = oe.eventMap.find(hit->nodeId); h && h->onPointerMove) {
          h->onPointerMove(hit->localPoint);
        }
        updateCursorForPoint(p);
        updateHoveredForPoint(p);
        return;
      }
    }
    if (auto hit = hitTestPointerTarget(eventMap_, graph, p)) {
      if (logThisMove) {
        std::fprintf(stderr,
                     "[flux:input] PointerMove hit-test path local=(%.1f,%.1f) ",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
        logNodeId("under cursor", hit->nodeId);
      }
      if (EventHandlers const* h = eventMap_.find(hit->nodeId); h && h->onPointerMove) {
        h->onPointerMove(hit->localPoint);
      }
    } else if (logThisMove) {
      std::fprintf(stderr, "[flux:input] PointerMove: no interactive node under cursor\n");
    }
    updateCursorForPoint(p);
    updateHoveredForPoint(p);
    return;
  }

  if (dbg) {
    std::fprintf(stderr, "[flux:input] PointerUp pos=(%.1f,%.1f)\n", static_cast<double>(p.x),
                 static_cast<double>(p.y));
  }
  HitTester tester{};
  std::optional<PressState> const released = activePress_;
  activePress_ = std::nullopt;
  if (released) {
    Application::instance().markReactiveDirty();
  }

  std::vector<std::unique_ptr<OverlayEntry>> const& entriesUp = window_.overlayManager().entries();
  for (auto it = entriesUp.rbegin(); it != entriesUp.rend(); ++it) {
    OverlayEntry const& oe = **it;
    Point const pl{p.x - oe.resolvedFrame.x, p.y - oe.resolvedFrame.y};
    if (auto hit = hitTestPointerTarget(oe.eventMap, oe.graph, pl)) {
      if (dbg) {
        std::fprintf(stderr,
                     "[flux:input] PointerUp release on interactive node local=(%.1f,%.1f) ",
                     static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
        logNodeId("release hit", hit->nodeId);
      }
      if (EventHandlers const* h = oe.eventMap.find(hit->nodeId)) {
        if (h->onPointerUp) {
          h->onPointerUp(hit->localPoint);
        }
        if (h->onTap && released && !released->cancelled) {
          bool const sameNode = released->nodeId == hit->nodeId;
          bool const retargetAfterRebuild =
              released->hadOnTapOnDown && oe.eventMap.find(released->nodeId) == nullptr &&
              !released->stableTargetKey.empty() && h->stableTargetKey == released->stableTargetKey;
          if (sameNode || retargetAfterRebuild) {
            pendingTapTargetKey_ = released->stableTargetKey;
            h->onTap();
            pendingTapTargetKey_.clear();
          }
        }
      }
      updateCursorForPoint(p);
      updateHoveredForPoint(p);
      return;
    }
  }

  auto hit = hitTestPointerTarget(eventMap_, graph, p);
  if (hit) {
    if (dbg) {
      std::fprintf(stderr,
                   "[flux:input] PointerUp release on interactive node local=(%.1f,%.1f) ",
                   static_cast<double>(hit->localPoint.x), static_cast<double>(hit->localPoint.y));
      logNodeId("release hit", hit->nodeId);
    }
    if (EventHandlers const* h = eventMap_.find(hit->nodeId)) {
      if (h->onPointerUp) {
        h->onPointerUp(hit->localPoint);
      }
      if (h->onTap && released && !released->cancelled) {
        bool const sameNode = released->nodeId == hit->nodeId;
        bool const retargetAfterRebuild =
            released->hadOnTapOnDown && eventMap_.find(released->nodeId) == nullptr &&
            !released->stableTargetKey.empty() && h->stableTargetKey == released->stableTargetKey;
        if (sameNode || retargetAfterRebuild) {
          pendingTapTargetKey_ = released->stableTargetKey;
          h->onTap();
          pendingTapTargetKey_.clear();
        }
      }
    }
  } else if (released) {
    if (dbg) {
      std::fprintf(stderr, "[flux:input] PointerUp: no interactive hit; trying press-target up\n");
      logNodeId("  released press", released->nodeId);
    }
    SceneGraph const& gRel = sceneGraphForPress(*released);
    auto const [currentId, pressed] = findPressHandlersWithNode(*released);
    if (pressed && pressed->onPointerUp && currentId.isValid()) {
      std::optional<Point> local = tester.localPointForNode(gRel, p, currentId);
      if (local) {
        if (dbg) {
          std::fprintf(stderr,
                       "[flux:input] PointerUp fallback local=(%.1f,%.1f)\n",
                       static_cast<double>(local->x), static_cast<double>(local->y));
        }
        pressed->onPointerUp(*local);
      } else {
        pressed->onPointerUp(Point{0.f, 0.f});
        if (dbg) {
          std::fprintf(stderr, "[flux:input] PointerUp fallback: localPointForNode FAILED; used (0,0)\n");
        }
      }
    }
  } else if (dbg) {
    std::fprintf(stderr, "[flux:input] PointerUp: no hit and no active press to notify\n");
  }
  updateCursorForPoint(p);
  updateHoveredForPoint(p);
}

bool useFocus() {
  Runtime* rt = Runtime::current();
  if (!rt) {
    return false;
  }
  StateStore* store = StateStore::current();
  if (!store) {
    return false;
  }
  return rt->isFocusInSubtree(store->currentComponentKey());
}

bool useKeyboardFocus() {
  Runtime* rt = Runtime::current();
  if (!rt) {
    return false;
  }
  StateStore* store = StateStore::current();
  if (!store) {
    return false;
  }
  return rt->isFocusInSubtree(store->currentComponentKey()) && rt->isLastFocusFromKeyboard();
}

bool useHover() {
  Runtime* rt = Runtime::current();
  if (!rt) {
    return false;
  }
  StateStore* store = StateStore::current();
  if (!store) {
    return false;
  }
  return rt->isHoverInSubtree(store->currentComponentKey());
}

bool usePress() {
  Runtime* rt = Runtime::current();
  if (!rt) {
    return false;
  }
  StateStore* store = StateStore::current();
  if (!store) {
    return false;
  }
  ComponentKey const& key = store->currentComponentKey();
  ComponentKey const& pressKey = rt->activePressKey();
  if (pressKey.empty() || key.size() > pressKey.size()) {
    return false;
  }
  if (!std::equal(key.begin(), key.end(), pressKey.begin())) {
    return false;
  }
  return rt->pressKeyMatchesStoreContext(*store);
}

std::function<void()> useRequestFocus() {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  assert(rt && "useRequestFocus called outside of a build pass");
  assert(store && "useRequestFocus called outside of a build pass");

  ComponentKey const key = store->currentComponentKey();

  return [rt, key] {
    rt->requestFocusInSubtree(key);
  };
}

void useViewAction(std::string const& name, std::function<void()> handler, std::function<bool()> isEnabled) {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  assert(rt && "useViewAction called outside of a build pass");
  assert(store && "useViewAction called outside of a build pass");
  if (store->overlayScope().has_value()) {
    assert(false && "useViewAction is not supported in overlay subtrees (ComponentKey collision risk)");
    return;
  }
  rt->actionRegistryForBuild().registerViewClaim(store->currentComponentKey(), name, std::move(handler),
                                                 std::move(isEnabled));
}

void useWindowAction(std::string const& name, std::function<void()> handler, std::function<bool()> isEnabled) {
  Runtime* rt = Runtime::current();
  assert(rt && "useWindowAction called outside of a build pass");
  rt->actionRegistryForBuild().registerWindowAction(name, std::move(handler), std::move(isEnabled));
}

namespace {

struct OverlayHookSlot {
  OverlayId id{};
  Window* window = nullptr;

  ~OverlayHookSlot() {
    if (id.isValid() && window) {
      OverlayId const rid = id;
      id = kInvalidOverlayId;
      window->removeOverlay(rid);
    }
  }
};

} // namespace

std::tuple<std::function<void(Element, OverlayConfig)>, std::function<void()>, bool> useOverlay() {
  StateStore* store = StateStore::current();
  Runtime* rt = Runtime::current();
  assert(store && "useOverlay called outside of a build pass");
  assert(rt && "useOverlay called outside of a build pass");

  OverlayHookSlot& slot = store->claimSlot<OverlayHookSlot>();
  Window& w = rt->window();
  slot.window = &w;

  OverlayHookSlot* slotPtr = &slot;
  Window* wPtr = &w;

  auto show = [slotPtr, wPtr](Element el, OverlayConfig cfg) {
    if (slotPtr->id.isValid()) {
      OverlayId const rid = slotPtr->id;
      slotPtr->id = kInvalidOverlayId;
      wPtr->removeOverlay(rid);
    }
    slotPtr->id = wPtr->pushOverlay(std::move(el), std::move(cfg));
  };

  auto hide = [slotPtr, wPtr]() {
    if (slotPtr->id.isValid()) {
      OverlayId const rid = slotPtr->id;
      slotPtr->id = kInvalidOverlayId;
      wPtr->removeOverlay(rid);
    }
  };

  bool const presented = slot.id.isValid();
  return {std::move(show), std::move(hide), presented};
}

std::optional<Rect> useLayoutRect() {
  Runtime* rt = Runtime::current();
  if (!rt) {
    return std::nullopt;
  }
  return rt->layoutRectForCurrentComponent();
}

} // namespace flux
