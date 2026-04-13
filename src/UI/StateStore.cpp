#include <Flux/UI/StateStore.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Detail/Runtime.hpp>

namespace flux {

namespace detail {

Runtime* currentRuntimeForInvalidation() noexcept {
  return Runtime::current();
}

std::function<void()> makeInvalidationCallback(Runtime* runtime, ComponentKey key, InvalidationKind kind) {
  return [runtime, key = std::move(key), kind]() {
    if (runtime) {
      runtime->invalidateSubtree(key, kind);
      return;
    }
    if (Application::hasInstance()) {
      Application::instance().markReactiveDirty();
    }
  };
}

} // namespace detail

thread_local StateStore* StateStore::sCurrent = nullptr;

StateStore* StateStore::current() noexcept { return sCurrent; }

void StateStore::setCurrent(StateStore* s) noexcept { sCurrent = s; }

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
  compositeElementModifierStack_.clear();
}

void StateStore::resetSlotCursors() {
  for (auto& [key, cs] : states_) {
    (void)key;
    cs.cursor = 0;
  }
}

void StateStore::pushComponent(ComponentKey const& key, std::type_index componentType) {
  visited_.insert(key);
  ComponentState& state = states_[key];
  if (state.componentType != componentType) {
    state.slots.clear();
    state.componentType = componentType;
  }
  state.cursor = 0;
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

} // namespace flux
