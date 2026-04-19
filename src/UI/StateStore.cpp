#include <Flux/UI/StateStore.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/UI/Element.hpp>

namespace flux {

namespace {

bool keyHasPrefix(ComponentKey const& key, ComponentKey const& prefix) {
  if (prefix.empty()) {
    return true;
  }
  if (key.size() < prefix.size()) {
    return false;
  }
  return std::equal(prefix.begin(), prefix.end(), key.begin());
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

void resetComponentStateStorage(ComponentState& state) {
  state.slots.clear();
  state.cursor = 0;
  state.componentType = std::type_index(typeid(void));
  state.lastVisitedEpoch = 0;
  state.lastBody = {nullptr, nullptr};
  state.lastBodyEpoch = 0;
  state.reusableConstraints.clear();
  state.reusableMeasures.clear();
  state.valueSnapshot = {};
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
  resetComponentStateStorage(state);
}

void StateStore::beginRebuild(bool forceFullRebuild) {
  ++buildEpoch_;
  forceFullRebuild_ = forceFullRebuild || pendingDirtyComposites_.empty();
  activeDirtyComposites_ = std::move(pendingDirtyComposites_);
  pendingDirtyComposites_.clear();
  activeStack_.clear();
  activeStateStack_.clear();
  compositePathStableStack_.clear();
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
  activeDirtyComposites_.clear();
  compositePathStableStack_.clear();
}

void StateStore::shutdown() {
  for (auto& [key, state] : states_) {
    (void)key;
    scrubOwnedObservableSubscriptions(states_, state);
    unsubscribeComponentState(state);
  }
  for (auto& [key, state] : states_) {
    (void)key;
    resetComponentStateStorage(state);
  }
  states_.clear();
  activeStack_.clear();
  activeStateStack_.clear();
  compositePathStableStack_.clear();
  compositeElementModifierStack_.clear();
  compositeConstraintStack_.clear();
  pendingDirtyComposites_.clear();
  activeDirtyComposites_.clear();
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

bool StateStore::isComponentDirty(ComponentKey const& key) const {
  return forceFullRebuild_ || activeDirtyComposites_.count(key) != 0;
}

bool StateStore::hasDirtyDescendant(ComponentKey const& key) const {
  if (forceFullRebuild_) {
    return true;
  }
  for (ComponentKey const& candidate : activeDirtyComposites_) {
    if (candidate.size() <= key.size()) {
      continue;
    }
    if (std::equal(key.begin(), key.end(), candidate.begin())) {
      return true;
    }
  }
  return false;
}

void StateStore::markRetainedSubtreeVisited(ComponentKey const& key) {
  for (auto& [candidateKey, state] : states_) {
    if (keyHasPrefix(candidateKey, key)) {
      state.lastVisitedEpoch = buildEpoch_;
    }
  }
}

void StateStore::markRetainedSubtreeVisited(ComponentState& state) {
  state.lastVisitedEpoch = buildEpoch_;
}

bool StateStore::currentCompositePathStable() const noexcept {
  if (compositePathStableStack_.empty()) {
    return true;
  }
  return compositePathStableStack_.back();
}

void StateStore::pushCompositePathStable(bool stable) {
  compositePathStableStack_.push_back(stable);
}

void StateStore::popCompositePathStable() {
  assert(!compositePathStableStack_.empty());
  compositePathStableStack_.pop_back();
}

bool StateStore::hasBodyForCurrentRebuild(ComponentKey const& key) const {
  auto const it = states_.find(key);
  return it != states_.end() && it->second.lastBody && it->second.lastBodyEpoch == buildEpoch_;
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

void StateStore::discardCurrentRebuildBody(ComponentKey const& key) {
  auto it = states_.find(key);
  if (it == states_.end() || it->second.lastBodyEpoch != buildEpoch_) {
    return;
  }
  it->second.lastBody = {nullptr, nullptr};
  it->second.lastBodyEpoch = 0;
}

void StateStore::recordBodyConstraints(ComponentKey const& key, LayoutConstraints const& constraints) {
  auto it = states_.find(key);
  if (it == states_.end()) {
    return;
  }
  ComponentState& state = it->second;
  for (LayoutConstraints const& recorded : state.reusableConstraints) {
    if (constraintsEqual(recorded, constraints)) {
      return;
    }
  }
  state.reusableConstraints.push_back(constraints);
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
