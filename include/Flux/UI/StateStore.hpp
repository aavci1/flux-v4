#pragma once

#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flux {

/// One heap-allocated state value (a Signal<T>, Animated<T>, or any type).
struct StateSlot {
  std::unique_ptr<void, void (*)(void*)> value{nullptr, nullptr};
  std::type_index type{typeid(void)};
};

/// State bucket for one component instance.
struct ComponentState {
  std::vector<StateSlot> slots;
  std::size_t cursor = 0; // reset to 0 at the start of each build pass
};

/// Owns all component state for the lifetime of the window.
/// One instance per Runtime. Not copyable or movable.
class StateStore {
public:
  StateStore() = default;
  StateStore(StateStore const&) = delete;
  StateStore& operator=(StateStore const&) = delete;

  /// Call before the build pass begins. Resets per-component cursors and
  /// clears the visited set.
  void beginRebuild();

  /// Call after the build pass completes. Destroys state for components
  /// whose keys were not visited in this pass (they were removed from the tree).
  void endRebuild();

  /// Explicitly destroy all state slots (triggers OverlayHookSlot destructors).
  /// Called by `BuildOrchestrator` destructor before other members are destroyed.
  void shutdown();

  /// Sets every component's slot cursor to 0 so a second pass over the tree
  /// (e.g. measure then build) can claim the same slots in the same order.
  void resetSlotCursors();

  /// Called by Element::Model<C>::build just before invoking body().
  /// Sets the active component key and resets that component's slot cursor.
  void pushComponent(ComponentKey const& key);

  /// Called by Element::Model<C>::build just after body() returns.
  void popComponent();

  /// Constraints for the composite whose `body()` is executing (`Element::Model<C>` build/measure).
  void pushCompositeConstraints(LayoutConstraints const& c);
  void popCompositeConstraints();
  LayoutConstraints const* currentCompositeConstraints() const;

  /// Called by useState<T> / useAnimated<T> inside body().
  /// Returns a reference to the next slot for the active component, creating
  /// it with `initial` if this is the first call.
  ///
  /// Slots are matched by call order, not by name — hooks must always be
  /// called in the same order every body() invocation.
  template<typename S, typename... Args>
  S& claimSlot(Args&&... args);

  /// Key of the composite component whose `body()` is executing (top of the active stack).
  ComponentKey const& currentComponentKey() const {
    assert(!activeStack_.empty());
    return activeStack_.back();
  }

  /// When building an overlay subtree, set to that overlay's id value (for `useFocus` / `useHover`).
  void setOverlayScope(std::optional<std::uint64_t> overlayIdValue) { overlayScope_ = overlayIdValue; }
  std::optional<std::uint64_t> overlayScope() const { return overlayScope_; }

  /// Thread-local pointer to the active StateStore during a build pass.
  /// Set by Runtime::rebuild; accessed by useState/useAnimated free functions.
  static StateStore* current() noexcept;
  static void setCurrent(StateStore* s) noexcept;

private:
  std::unordered_map<ComponentKey, ComponentState, ComponentKeyHash> states_;
  std::unordered_set<ComponentKey, ComponentKeyHash> visited_;

  // Stack of active component keys (depth > 1 when body() calls a helper
  // that returns an Element containing further composites — rare but valid).
  std::vector<ComponentKey> activeStack_;

  std::optional<std::uint64_t> overlayScope_{};

  std::vector<LayoutConstraints> compositeConstraintStack_{};

  static thread_local StateStore* sCurrent;
};

} // namespace flux

// --- template implementation ---

namespace flux {

template<typename S, typename... Args>
S& StateStore::claimSlot(Args&&... args) {
  assert(!activeStack_.empty() && "useState called outside of body()");
  ComponentKey const& key = activeStack_.back();
  visited_.insert(key);

  ComponentState& cs = states_[key];
  std::size_t idx = cs.cursor++;

  if (idx < cs.slots.size()) {
    // Existing slot — verify type then return.
    assert(cs.slots[idx].type == std::type_index(typeid(S)) &&
           "useState<T> call order changed between builds — "
           "hooks must always be called in the same order");
    return *static_cast<S*>(cs.slots[idx].value.get());
  }

  // New slot — construct in place (Signal/Animated are non-movable).
  S* raw = new S(std::forward<Args>(args)...);
  cs.slots.push_back(StateSlot{
      std::unique_ptr<void, void (*)(void*)>(raw, [](void* p) {
        delete static_cast<S*>(p);
      }),
      std::type_index(typeid(S))});
  return *raw;
}

} // namespace flux
