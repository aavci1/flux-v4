#include <Flux/UI/StateStore.hpp>

namespace flux {

thread_local StateStore* StateStore::sCurrent = nullptr;

StateStore* StateStore::current() noexcept { return sCurrent; }

void StateStore::setCurrent(StateStore* s) noexcept { sCurrent = s; }

void StateStore::beginRebuild() {
  compositeConstraintStack_.clear();
  visited_.clear();
  for (auto& [key, cs] : states_) {
    (void)key;
    cs.cursor = 0;
  }
}

void StateStore::endRebuild() {
  for (auto it = states_.begin(); it != states_.end();) {
    if (!visited_.count(it->first)) {
      it = states_.erase(it);
    } else {
      ++it;
    }
  }
}

void StateStore::shutdown() {
  states_.clear();
  visited_.clear();
  activeStack_.clear();
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

} // namespace flux
