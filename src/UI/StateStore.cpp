#include <Flux/UI/StateStore.hpp>

#include <Flux/Core/Application.hpp>

#include <algorithm>

namespace flux {

namespace {

std::unordered_map<void*, StateStore*> gSlotStoreRegistry;

} // namespace

thread_local StateStore* StateStore::sCurrent = nullptr;

StateStore* StateStore::current() noexcept { return sCurrent; }

void StateStore::setCurrent(StateStore* s) noexcept { sCurrent = s; }

void StateStore::registerSlotOwner(void* slot, ComponentKey const& key) {
  slotOwners_[slot] = key;
  gSlotStoreRegistry[slot] = this;
}

void StateStore::releaseAllSlotPointers(ComponentState& cs) {
  for (auto& sl : cs.slots) {
    if (!sl.value) {
      continue;
    }
    void* p = sl.value.get();
    slotOwners_.erase(p);
    gSlotStoreRegistry.erase(p);
  }
}

void StateStore::beginRebuild() {
  compositeConstraintStack_.clear();
  compositeElementModifierStack_.clear();
  visited_.clear();
  for (auto& [key, cs] : states_) {
    (void)key;
    cs.cursor = 0;
  }
}

void StateStore::endRebuild() {
  for (auto it = states_.begin(); it != states_.end();) {
    if (!visited_.count(it->first)) {
      releaseAllSlotPointers(it->second);
      it = states_.erase(it);
    } else {
      ++it;
    }
  }
}

void StateStore::shutdown() {
  for (auto& [key, cs] : states_) {
    (void)key;
    releaseAllSlotPointers(cs);
  }
  states_.clear();
  visited_.clear();
  activeStack_.clear();
  compositeElementModifierStack_.clear();
  slotOwners_.clear();
  dirtyKeys_.clear();
}

void StateStore::resetSlotCursors() {
  for (auto& [key, cs] : states_) {
    (void)key;
    cs.cursor = 0;
  }
}

void StateStore::pushComponent(ComponentKey const& key) {
  visited_.insert(key);
  states_[key].cursor = 0;
  activeStack_.push_back(key);
}

void StateStore::popComponent() {
  assert(!activeStack_.empty());
  activeStack_.pop_back();
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

void StateStore::pushCompositeElementModifiers(ElementModifiers const* m) {
  compositeElementModifierStack_.push_back(m);
}

void StateStore::popCompositeElementModifiers() {
  assert(!compositeElementModifierStack_.empty());
  compositeElementModifierStack_.pop_back();
}

ElementModifiers const* StateStore::currentCompositeElementModifiers() const noexcept {
  if (compositeElementModifierStack_.empty()) {
    return nullptr;
  }
  return compositeElementModifierStack_.back();
}

ComponentKey const& StateStore::currentComponentKey() const {
  assert(!activeStack_.empty());
  return activeStack_.back();
}

void StateStore::setOverlayScope(std::optional<std::uint64_t> overlayIdValue) {
  overlayScope_ = overlayIdValue;
}

std::optional<std::uint64_t> StateStore::overlayScope() const {
  return overlayScope_;
}

void StateStore::markFullRebuild() {
  fullRebuildRequired_ = true;
}

void StateStore::markSlotDirty(void* slot) {
  auto it = slotOwners_.find(slot);
  if (it == slotOwners_.end()) {
    markFullRebuild();
  } else {
    ComponentKey const& key = it->second;
    if (std::find(dirtyKeys_.begin(), dirtyKeys_.end(), key) == dirtyKeys_.end()) {
      dirtyKeys_.push_back(key);
    }
  }
  if (!Application::hasInstance()) {
    return;
  }
  Application::instance().requestRebuild();
}

void StateStore::clearDirtyState() {
  dirtyKeys_.clear();
  fullRebuildRequired_ = false;
}

void StateStore::markSlotDirtyFromBridge(void* slot) {
  auto it = gSlotStoreRegistry.find(slot);
  if (it == gSlotStoreRegistry.end()) {
    if (Application::hasInstance()) {
      Application::instance().requestRebuild();
    }
    return;
  }
  it->second->markSlotDirty(slot);
}

void StateStore::beginPartialRebuild(ComponentKey const& key) {
  savedActiveStack_ = activeStack_;
  inPartialRebuild_ = true;
  pushComponent(key);
}

void StateStore::endPartialRebuild() {
  popComponent();
  activeStack_ = std::move(savedActiveStack_);
  savedActiveStack_.clear();
  inPartialRebuild_ = false;
}

} // namespace flux
