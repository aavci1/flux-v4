#include <Flux/UI/StateStore.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/SceneGraph/InteractionData.hpp>
#include <Flux/UI/Element.hpp>

#include "Debug/PerfCounters.hpp"

namespace flux {

namespace {

bool keyHasPrefix(ComponentKey const& key, ComponentKey const& prefix) {
  if (prefix.empty()) {
    return true;
  }
  if (key.size() < prefix.size()) {
    return false;
  }
  debug::perf::recordComponentKeyPrefixCompare(prefix.size());
  return key.hasPrefix(prefix);
}

template<typename Signature>
void assignCallbackCell(std::shared_ptr<std::function<Signature>>& cell,
                        std::function<Signature> const& value) {
  if (!cell) {
    cell = std::make_shared<std::function<Signature>>();
  }
  *cell = value;
}

template<typename... Args>
auto makeCallbackForwarder(std::shared_ptr<std::function<void(Args...)>> cell) {
  return [cell = std::move(cell)](Args... args) {
    if (cell && *cell) {
      (*cell)(std::forward<Args>(args)...);
    }
  };
}

void unsubscribeComponentState(ComponentState& state) {
  std::vector<ComponentSubscription> subscriptions = std::move(state.subscriptions);
  state.subscriptions.clear();
  for (ComponentSubscription const& sub : subscriptions) {
    if (sub.observable) {
      sub.observable->unobserve(sub.handle);
    }
  }
}

void unsubscribeOwnedSlots(ComponentState& state) {
  for (StateSlot& slot : state.slots) {
    if (slot.observable && slot.ownerHandle.isValid()) {
      slot.observable->unobserve(slot.ownerHandle);
      slot.ownerHandle = {};
    }
  }
}

void resetComponentStateStorage(ComponentState& state) {
  unsubscribeOwnedSlots(state);
  state.slots.clear();
  state.cursor = 0;
  state.componentType = std::type_index(typeid(void));
  state.lastVisitedEpoch = 0;
  state.lastBody = {nullptr, nullptr};
  state.lastBodyEpoch = 0;
  state.lastBodyStructurallyStable = false;
  state.lastBodyConstraints.reset();
  state.reusableMeasures.clear();
  state.environmentDependencies.clear();
  state.pendingEnvironmentDependencies.clear();
  state.lastSceneElement.reset();
  state.lastBuildSnapshot.reset();
}

void scrubObservableSubscriptions(std::unordered_map<ComponentKey, ComponentState, ComponentKeyHash>& states,
                                  Observable const* observable) {
  if (!observable) {
    return;
  }
  for (auto& [key, candidate] : states) {
    (void)key;
    std::erase_if(candidate.subscriptions, [observable](ComponentSubscription const& sub) {
      return sub.observable == observable;
    });
  }
}

void scrubOwnedObservableSubscriptions(std::unordered_map<ComponentKey, ComponentState, ComponentKeyHash>& states,
                                       ComponentState const& owner) {
  for (StateSlot const& slot : owner.slots) {
    scrubObservableSubscriptions(states, slot.observable);
  }
}

} // namespace

thread_local StateStore* StateStore::sCurrent = nullptr;

StateStore::~StateStore() {
  shutdown();
}

StateStore* StateStore::current() noexcept { return sCurrent; }

void StateStore::setCurrent(StateStore* s) noexcept { sCurrent = s; }

bool StateStore::constraintsEqual(LayoutConstraints const& a, LayoutConstraints const& b) noexcept {
  return a.minWidth == b.minWidth && a.minHeight == b.minHeight &&
         a.maxWidth == b.maxWidth && a.maxHeight == b.maxHeight;
}

bool StateStore::rectEqual(Rect const& a, Rect const& b) noexcept {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

void StateStore::clearComponentState(ComponentState& state) {
  scrubOwnedObservableSubscriptions(states_, state);
  unsubscribeComponentState(state);
  unsubscribeOwnedSlots(state);
  resetComponentStateStorage(state);
}

bool StateStore::environmentDependenciesMatch(ComponentState const& state) const {
  for (EnvironmentValueSnapshot const& dep : state.environmentDependencies) {
    if (!dep.value || !dep.equalsCurrent || !dep.equalsCurrent(dep.value.get(), EnvironmentStack::current())) {
      return false;
    }
  }
  return true;
}

void StateStore::beginRebuild(bool forceFullRebuild) {
  ++buildEpoch_;
  forceFullRebuild_ = forceFullRebuild || pendingDirtyComposites_.empty();
  activeDirtyComposites_ = std::move(pendingDirtyComposites_);
  activeDirtyAncestorKeys_.clear();
  if (!forceFullRebuild_) {
    for (ComponentKey const& dirtyKey : activeDirtyComposites_) {
      ComponentKey ancestor = dirtyKey;
      while (!ancestor.empty()) {
        ancestor.pop_back();
        activeDirtyAncestorKeys_.insert(ancestor);
      }
    }
  }
  pendingDirtyComposites_.clear();
  activeStack_.clear();
  activeStateStack_.clear();
  compositeConstraintStack_.clear();
  compositeElementModifierStack_.clear();
  for (auto& [key, cs] : states_) {
    (void)key;
    cs.cursor = 0;
  }
}

void StateStore::endRebuild() {
  std::vector<ComponentKey> staleKeys{};
  staleKeys.reserve(states_.size());
  for (auto it = states_.begin(); it != states_.end();) {
    if (it->second.lastVisitedEpoch != buildEpoch_) {
      staleKeys.push_back(it->first);
    }
    ++it;
  }
  for (ComponentKey const& key : staleKeys) {
    auto it = states_.find(key);
    if (it != states_.end()) {
      scrubOwnedObservableSubscriptions(states_, it->second);
      unsubscribeComponentState(it->second);
      unsubscribeOwnedSlots(it->second);
    }
  }
  for (ComponentKey const& key : staleKeys) {
    auto it = states_.find(key);
    if (it == states_.end()) {
      continue;
    }
    resetComponentStateStorage(it->second);
    states_.erase(it);
  }
  std::erase_if(interactions_, [&](auto const& entry) {
    return entry.second.lastVisitedEpoch != buildEpoch_;
  });
  std::erase_if(modifierLayers_, [&](auto const& entry) {
    return states_.find(entry.first) == states_.end();
  });
  activeDirtyComposites_.clear();
  activeDirtyAncestorKeys_.clear();
}

void StateStore::shutdown() {
  for (auto& [key, state] : states_) {
    (void)key;
    scrubOwnedObservableSubscriptions(states_, state);
    unsubscribeComponentState(state);
    unsubscribeOwnedSlots(state);
  }
  for (auto& [key, state] : states_) {
    (void)key;
    resetComponentStateStorage(state);
  }
  states_.clear();
  interactions_.clear();
  modifierLayers_.clear();
  activeStack_.clear();
  activeStateStack_.clear();
  compositeElementModifierStack_.clear();
  compositeConstraintStack_.clear();
  pendingDirtyComposites_.clear();
  activeDirtyComposites_.clear();
  activeDirtyAncestorKeys_.clear();
}

void StateStore::resetSlotCursors() {
  for (auto& [key, cs] : states_) {
    (void)key;
    cs.cursor = 0;
  }
}

void StateStore::pushComponent(ComponentKey const& key, std::type_index componentType) {
  auto [it, inserted] = states_.try_emplace(key);
  ComponentState& state = it->second;
  if (state.componentType != componentType) {
    clearComponentState(state);
    state.componentType = componentType;
  }
  state.lastVisitedEpoch = buildEpoch_;
  state.cursor = 0;
  state.pendingEnvironmentDependencies.clear();
  activeStack_.push_back(&it->first);
  activeStateStack_.push_back(&state);
}

void StateStore::popComponent() {
  assert(!activeStack_.empty());
  activeStack_.pop_back();
  assert(!activeStateStack_.empty());
  activeStateStack_.pop_back();
}

void StateStore::pushCompositeConstraints(LayoutConstraints const& c) {
  compositeConstraintStack_.push_back(c);
}

void StateStore::popCompositeConstraints() {
  if (!compositeConstraintStack_.empty()) {
    compositeConstraintStack_.pop_back();
  }
}

LayoutConstraints const* StateStore::currentCompositeConstraints() const {
  if (compositeConstraintStack_.empty()) {
    return nullptr;
  }
  return &compositeConstraintStack_.back();
}

void StateStore::pushCompositeElementModifiers(detail::ElementModifiers const* m) {
  compositeElementModifierStack_.push_back(m);
}

void StateStore::popCompositeElementModifiers() {
  assert(!compositeElementModifierStack_.empty());
  compositeElementModifierStack_.pop_back();
}

detail::ElementModifiers const* StateStore::currentCompositeElementModifiers() const noexcept {
  if (compositeElementModifierStack_.empty()) {
    return nullptr;
  }
  return compositeElementModifierStack_.back();
}

ComponentKey const& StateStore::currentComponentKey() const {
  assert(!activeStack_.empty());
  return *activeStack_.back();
}

void StateStore::markCompositeDirty(ComponentKey const& key) {
  pendingDirtyComposites_.insert(key);
  if (Application::hasInstance()) {
    Application::instance().markReactiveDirty();
  }
}

bool StateStore::hasPendingDirtyComponents() const noexcept {
  return !pendingDirtyComposites_.empty();
}

std::vector<ComponentKey> StateStore::pendingDirtyComponents() const {
  std::vector<ComponentKey> keys;
  keys.reserve(pendingDirtyComposites_.size());
  for (ComponentKey const& key : pendingDirtyComposites_) {
    keys.push_back(key);
  }
  return keys;
}

bool StateStore::isComponentDirty(ComponentKey const& key) const {
  return forceFullRebuild_ || activeDirtyComposites_.count(key) != 0;
}

bool StateStore::hasDirtyDescendant(ComponentKey const& key) const {
  if (forceFullRebuild_) {
    return true;
  }
  return activeDirtyAncestorKeys_.count(key) != 0;
}

void StateStore::markComponentsOutsideSubtreeVisited(ComponentKey const& key) {
  for (auto& [candidateKey, state] : states_) {
    if (!keyHasPrefix(candidateKey, key)) {
      state.lastVisitedEpoch = buildEpoch_;
    }
  }
  for (auto& [candidateKey, interaction] : interactions_) {
    if (!keyHasPrefix(candidateKey, key)) {
      interaction.lastVisitedEpoch = buildEpoch_;
    }
  }
}

void StateStore::markRetainedSubtreeVisited(ComponentKey const& key) {
  for (auto& [candidateKey, state] : states_) {
    if (keyHasPrefix(candidateKey, key)) {
      state.lastVisitedEpoch = buildEpoch_;
    }
  }
  for (auto& [candidateKey, interaction] : interactions_) {
    if (keyHasPrefix(candidateKey, key)) {
      interaction.lastVisitedEpoch = buildEpoch_;
    }
  }
}

void StateStore::markRetainedSubtreeVisited(ComponentState& state) {
  state.lastVisitedEpoch = buildEpoch_;
}

bool StateStore::hasBodyForCurrentRebuild(ComponentKey const& key,
                                          LayoutConstraints const& constraints) const {
  auto const it = states_.find(key);
  return it != states_.end() && it->second.lastBody && it->second.lastBodyEpoch == buildEpoch_ &&
         it->second.lastBodyConstraints.has_value() &&
         constraintsEqual(*it->second.lastBodyConstraints, constraints);
}

bool StateStore::bodyStructurallyStable(ComponentKey const& key) const {
  auto const it = states_.find(key);
  return it != states_.end() && it->second.lastBody && it->second.lastBodyEpoch == buildEpoch_ &&
         it->second.lastBodyStructurallyStable;
}

Element* StateStore::cachedBody(ComponentKey const& key) {
  auto it = states_.find(key);
  if (it == states_.end() || !it->second.lastBody) {
    return nullptr;
  }
  return static_cast<Element*>(it->second.lastBody.get());
}

Element const* StateStore::cachedBody(ComponentKey const& key) const {
  auto it = states_.find(key);
  if (it == states_.end() || !it->second.lastBody) {
    return nullptr;
  }
  return static_cast<Element const*>(it->second.lastBody.get());
}

void StateStore::recordBodyStability(ComponentKey const& key, bool stable) {
  auto it = states_.find(key);
  if (it == states_.end()) {
    return;
  }
  it->second.lastBodyStructurallyStable = stable;
}

Element const* StateStore::sceneElement(ComponentKey const& key) const {
  auto it = states_.find(key);
  if (it == states_.end() || !it->second.lastSceneElement) {
    return nullptr;
  }
  return static_cast<Element const*>(it->second.lastSceneElement.get());
}

void StateStore::discardCurrentRebuildBody(ComponentKey const& key) {
  auto it = states_.find(key);
  if (it == states_.end() || it->second.lastBodyEpoch != buildEpoch_) {
    return;
  }
  it->second.lastBody = {nullptr, nullptr};
  it->second.lastBodyEpoch = 0;
  it->second.lastBodyStructurallyStable = false;
  it->second.lastBodyConstraints.reset();
}

std::optional<ComponentBuildSnapshot> StateStore::buildSnapshot(ComponentKey const& key) const {
  auto it = states_.find(key);
  if (it == states_.end()) {
    return std::nullopt;
  }
  return it->second.lastBuildSnapshot;
}

void StateStore::recordBuildSnapshot(ComponentKey const& key,
                                     LayoutConstraints const& constraints,
                                     LayoutHints const& hints, Point origin,
                                     Size assignedSize, bool hasAssignedWidth,
                                     bool hasAssignedHeight, Point rootPosition) {
  auto it = states_.find(key);
  if (it == states_.end()) {
    return;
  }
  it->second.lastBuildSnapshot = ComponentBuildSnapshot{
      .constraints = constraints,
      .hints = hints,
      .origin = origin,
      .rootPosition = rootPosition,
      .assignedSize = assignedSize,
      .hasAssignedWidth = hasAssignedWidth,
      .hasAssignedHeight = hasAssignedHeight,
  };
}

void StateStore::recordSceneElement(ComponentKey const& key, Element const& element) {
  auto it = states_.find(key);
  if (it == states_.end()) {
    return;
  }
  Element* raw = new Element(element);
  it->second.lastSceneElement = std::unique_ptr<void, void (*)(void*)>(
      raw, [](void* p) { delete static_cast<Element*>(p); });
}

void StateStore::recordCompositeBodyResolve(bool comparedPreviousBody, bool structurallyStable,
                                            bool legacyPredicateWouldHaveMatched) {
  debug::perf::recordCompositeBodyResolve(comparedPreviousBody, structurallyStable,
                                          legacyPredicateWouldHaveMatched);
}

bool StateStore::modifierLayersStructurallyStable(
    ComponentKey const& key, std::span<detail::ElementModifiers const> layers) const {
  auto const it = modifierLayers_.find(key);
  if (it == modifierLayers_.end() || it->second.size() != layers.size()) {
    return false;
  }
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (!detail::elementModifiersStructurallyEqual(it->second[i], layers[i])) {
      return false;
    }
  }
  return true;
}

void StateStore::recordModifierLayers(ComponentKey const& key,
                                      std::span<detail::ElementModifiers const> layers) {
  if (key.empty()) {
    return;
  }
  modifierLayers_[key].assign(layers.begin(), layers.end());
}

void StateStore::recordInteraction(ComponentKey const& key,
                                   detail::ElementModifiers const& modifiers) {
  if (key.empty() || !modifiers.hasInteraction()) {
    return;
  }
  InteractionCallbackCells& cells = interactions_[key];
  cells.lastVisitedEpoch = buildEpoch_;
  if (modifiers.onPointerDown || cells.onPointerDown) {
    assignCallbackCell(cells.onPointerDown, modifiers.onPointerDown);
  }
  if (modifiers.onPointerUp || cells.onPointerUp) {
    assignCallbackCell(cells.onPointerUp, modifiers.onPointerUp);
  }
  if (modifiers.onPointerMove || cells.onPointerMove) {
    assignCallbackCell(cells.onPointerMove, modifiers.onPointerMove);
  }
  if (modifiers.onScroll || cells.onScroll) {
    assignCallbackCell(cells.onScroll, modifiers.onScroll);
  }
  if (modifiers.onKeyDown || cells.onKeyDown) {
    assignCallbackCell(cells.onKeyDown, modifiers.onKeyDown);
  }
  if (modifiers.onKeyUp || cells.onKeyUp) {
    assignCallbackCell(cells.onKeyUp, modifiers.onKeyUp);
  }
  if (modifiers.onTextInput || cells.onTextInput) {
    assignCallbackCell(cells.onTextInput, modifiers.onTextInput);
  }
  if (modifiers.onTap || cells.onTap) {
    assignCallbackCell(cells.onTap, modifiers.onTap);
  }
}

std::unique_ptr<scenegraph::InteractionData>
StateStore::makeInteractionData(ComponentKey const& key,
                                detail::ElementModifiers const& modifiers) {
  if (!modifiers.hasInteraction()) {
    return nullptr;
  }
  recordInteraction(key, modifiers);
  InteractionCallbackCells* cells = nullptr;
  if (auto it = interactions_.find(key); it != interactions_.end()) {
    cells = &it->second;
  }

  auto data = std::make_unique<scenegraph::InteractionData>();
  data->stableTargetKey = key;
  data->cursor = modifiers.cursor;
  data->focusable = modifiers.focusable || static_cast<bool>(modifiers.onKeyDown) ||
                    static_cast<bool>(modifiers.onKeyUp) ||
                    static_cast<bool>(modifiers.onTextInput);

  if (cells && modifiers.onPointerDown && cells->onPointerDown) {
    data->onPointerDown = makeCallbackForwarder(cells->onPointerDown);
  } else {
    data->onPointerDown = modifiers.onPointerDown;
  }
  if (cells && modifiers.onPointerUp && cells->onPointerUp) {
    data->onPointerUp = makeCallbackForwarder(cells->onPointerUp);
  } else {
    data->onPointerUp = modifiers.onPointerUp;
  }
  if (cells && modifiers.onPointerMove && cells->onPointerMove) {
    data->onPointerMove = makeCallbackForwarder(cells->onPointerMove);
  } else {
    data->onPointerMove = modifiers.onPointerMove;
  }
  if (cells && modifiers.onScroll && cells->onScroll) {
    data->onScroll = makeCallbackForwarder(cells->onScroll);
  } else {
    data->onScroll = modifiers.onScroll;
  }
  if (cells && modifiers.onKeyDown && cells->onKeyDown) {
    data->onKeyDown = makeCallbackForwarder(cells->onKeyDown);
  } else {
    data->onKeyDown = modifiers.onKeyDown;
  }
  if (cells && modifiers.onKeyUp && cells->onKeyUp) {
    data->onKeyUp = makeCallbackForwarder(cells->onKeyUp);
  } else {
    data->onKeyUp = modifiers.onKeyUp;
  }
  if (cells && modifiers.onTextInput && cells->onTextInput) {
    data->onTextInput = makeCallbackForwarder(cells->onTextInput);
  } else {
    data->onTextInput = modifiers.onTextInput;
  }
  if (cells && modifiers.onTap && cells->onTap) {
    data->onTap = makeCallbackForwarder(cells->onTap);
  } else {
    data->onTap = modifiers.onTap;
  }

  if (data->isEmpty()) {
    return nullptr;
  }
  return data;
}

bool StateStore::hasInteractionDescendant(ComponentKey const& key) const {
  for (auto const& [candidateKey, cells] : interactions_) {
    (void)cells;
    if (keyHasPrefix(candidateKey, key)) {
      return true;
    }
  }
  return false;
}

ComponentState const* StateStore::findComponentState(ComponentKey const& key) const {
  auto const it = states_.find(key);
  return it == states_.end() ? nullptr : &it->second;
}

ComponentState* StateStore::findComponentState(ComponentKey const& key) {
  auto const it = states_.find(key);
  return it == states_.end() ? nullptr : &it->second;
}

std::optional<Size> StateStore::cachedMeasure(ComponentKey const& key,
                                              LayoutConstraints const& constraints) const {
  auto const it = states_.find(key);
  if (it == states_.end()) {
    return std::nullopt;
  }
  for (auto const& [recordedConstraints, size] : it->second.reusableMeasures) {
    if (constraintsEqual(recordedConstraints, constraints)) {
      return size;
    }
  }
  return std::nullopt;
}

void StateStore::recordMeasure(ComponentKey const& key, LayoutConstraints const& constraints, Size size) {
  ComponentState& state = states_[key];
  for (auto& [recordedConstraints, recordedSize] : state.reusableMeasures) {
    if (constraintsEqual(recordedConstraints, constraints)) {
      recordedSize = size;
      return;
    }
  }
  state.reusableMeasures.emplace_back(constraints, size);
}

void StateStore::setOverlayScope(std::optional<std::uint64_t> overlayIdValue) {
  overlayScope_ = overlayIdValue;
}

std::optional<std::uint64_t> StateStore::overlayScope() const {
  return overlayScope_;
}

} // namespace flux
