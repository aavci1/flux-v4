#pragma once

/// \file Flux/UI/StateStore.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/Reactive/Detail/TypeTraits.hpp>

#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flux {

struct ElementModifiers;
class Element;

/// One heap-allocated state value (a Signal<T>, Animation<T>, or any type).
struct StateSlot {
  std::unique_ptr<void, void (*)(void*)> value{nullptr, nullptr};
  std::type_index type{typeid(void)};
};

struct ComponentSubscription {
  Observable* observable = nullptr;
  ObserverHandle handle{};
};

struct ComponentValueSnapshot {
  std::unique_ptr<void, void (*)(void*)> value{nullptr, nullptr};
  bool (*equals)(void const*, void const*) = nullptr;
  std::type_index type{typeid(void)};
};

/// State bucket for one component instance.
struct ComponentState {
  std::deque<StateSlot> slots;
  std::size_t cursor = 0; // reset to 0 at the start of each build pass
  std::type_index componentType{typeid(void)};
  std::unique_ptr<void, void (*)(void*)> lastBody{nullptr, nullptr};
  std::uint64_t lastBodyEpoch = 0;
  std::vector<LayoutConstraints> reusableConstraints;
  std::vector<std::pair<LayoutConstraints, Size>> reusableMeasures;
  ComponentValueSnapshot valueSnapshot{};
  std::vector<ComponentSubscription> subscriptions;
};

/// Owns all component state for the lifetime of the window.
/// One instance per Runtime. Not copyable or movable.
class StateStore {
public:
  StateStore() = default;
  ~StateStore();
  StateStore(StateStore const&) = delete;
  StateStore& operator=(StateStore const&) = delete;

  /// Call before the build pass begins. Resets per-component cursors and
  /// clears the visited set.
  void beginRebuild(bool forceFullRebuild = true);

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
  void pushComponent(ComponentKey const& key, std::type_index componentType);

  /// Called by Element::Model<C>::build just after body() returns.
  void popComponent();

  /// Constraints for the composite whose `body()` is executing (`Element::Model<C>` build/measure).
  void pushCompositeConstraints(LayoutConstraints const& c);
  void popCompositeConstraints();
  LayoutConstraints const* currentCompositeConstraints() const;

  /// Outer \ref Element modifiers while the composite's \c body() runs (paired with \c pushComponent).
  void pushCompositeElementModifiers(ElementModifiers const* m);
  void popCompositeElementModifiers();
  ElementModifiers const* currentCompositeElementModifiers() const noexcept;

  /// Called by useState<T> / useAnimation<T> inside body().
  /// Returns a reference to the next slot for the active component, creating
  /// it with `initial` if this is the first call.
  ///
  /// Slots are matched by call order, not by name — hooks must always be
  /// called in the same order every body() invocation.
  template<typename S, typename... Args>
  S& claimSlot(Args&&... args);

  /// Key of the composite component whose `body()` is executing (top of the active stack).
  ComponentKey const& currentComponentKey() const;

  /// Marks one composite dirty for the next rebuild.
  void markCompositeDirty(ComponentKey const& key);

  [[nodiscard]] bool hasPendingDirtyComponents() const noexcept;
  [[nodiscard]] bool shouldForceFullRebuild() const noexcept { return forceFullRebuild_; }
  [[nodiscard]] bool isComponentDirty(ComponentKey const& key) const;
  [[nodiscard]] bool hasDirtyDescendant(ComponentKey const& key) const;
  void markRetainedSubtreeVisited(ComponentKey const& key);
  [[nodiscard]] bool currentCompositePathStable() const noexcept;
  void pushCompositePathStable(bool stable);
  void popCompositePathStable();

  template<typename C>
  bool canReuseBody(ComponentKey const& key, C const& value, LayoutConstraints const& constraints) const;

  [[nodiscard]] bool hasBodyForCurrentRebuild(ComponentKey const& key) const;
  Element* cachedBody(ComponentKey const& key);
  Element const* cachedBody(ComponentKey const& key) const;
  void recordBodyConstraints(ComponentKey const& key, LayoutConstraints const& constraints);
  std::optional<Size> cachedMeasure(ComponentKey const& key, LayoutConstraints const& constraints) const;
  void recordMeasure(ComponentKey const& key, LayoutConstraints const& constraints, Size size);

  template<typename C>
  Element& commitBody(ComponentKey const& key, C const& value, LayoutConstraints const& constraints,
                      std::unique_ptr<Element> body, std::vector<Observable*> deps);

  /// When building an overlay subtree, set to that overlay's id value (for `useFocus` / `useHover`).
  void setOverlayScope(std::optional<std::uint64_t> overlayIdValue);
  std::optional<std::uint64_t> overlayScope() const;

  /// Thread-local pointer to the active StateStore during a build pass.
  /// Set by Runtime::rebuild; accessed by useState/useAnimation free functions.
  static StateStore* current() noexcept;
  static void setCurrent(StateStore* s) noexcept;

private:
  std::unordered_map<ComponentKey, ComponentState, ComponentKeyHash> states_;
  std::unordered_set<ComponentKey, ComponentKeyHash> visited_;

  // Stack of active component keys (depth > 1 when body() calls a helper
  // that returns an Element containing further composites — rare but valid).
  std::vector<ComponentKey const*> activeStack_;
  std::vector<ComponentState*> activeStateStack_;
  std::vector<bool> compositePathStableStack_{};

  std::optional<std::uint64_t> overlayScope_{};

  std::vector<LayoutConstraints> compositeConstraintStack_{};
  std::vector<ElementModifiers const*> compositeElementModifierStack_{};
  std::unordered_set<ComponentKey, ComponentKeyHash> pendingDirtyComposites_{};
  std::unordered_set<ComponentKey, ComponentKeyHash> activeDirtyComposites_{};
  bool forceFullRebuild_ = true;
  std::uint64_t buildEpoch_ = 0;

  static thread_local StateStore* sCurrent;

  static bool constraintsEqual(LayoutConstraints const& a, LayoutConstraints const& b) noexcept;
  static bool rectEqual(Rect const& a, Rect const& b) noexcept;
  static void clearComponentState(ComponentState& state);

  template<typename C>
  static ComponentValueSnapshot makeValueSnapshot(C const& value);
};

} // namespace flux

// --- template implementation ---

namespace flux {

template<typename C>
ComponentValueSnapshot StateStore::makeValueSnapshot(C const& value) {
  if constexpr (std::is_copy_constructible_v<C>) {
    ComponentValueSnapshot snapshot{};
    C* raw = new C(value);
    snapshot.value = std::unique_ptr<void, void (*)(void*)>(
        raw, [](void* p) { delete static_cast<C*>(p); });
    if constexpr (detail::equalityComparableV<C>) {
      snapshot.equals = [](void const* lhs, void const* rhs) {
        return *static_cast<C const*>(lhs) == *static_cast<C const*>(rhs);
      };
    } else if constexpr (std::is_trivially_copyable_v<C>) {
      snapshot.equals = [](void const* lhs, void const* rhs) {
        return std::memcmp(lhs, rhs, sizeof(C)) == 0;
      };
    }
    snapshot.type = std::type_index(typeid(C));
    return snapshot;
  } else {
    (void)value;
    return {};
  }
}

template<typename S, typename... Args>
S& StateStore::claimSlot(Args&&... args) {
  assert(!activeStack_.empty() && "useState called outside of body()");
  ComponentState& cs = *activeStateStack_.back();
  std::size_t idx = cs.cursor++;

  if (idx < cs.slots.size()) {
    // Existing slot — verify type then return.
    assert(cs.slots[idx].type == std::type_index(typeid(S)) &&
           "useState<T> call order changed between builds — "
           "hooks must always be called in the same order");
    return *static_cast<S*>(cs.slots[idx].value.get());
  }

  // New slot — construct in place (Signal/Animation are non-movable).
  S* raw = new S(std::forward<Args>(args)...);
  cs.slots.push_back(StateSlot{
      std::unique_ptr<void, void (*)(void*)>(raw, [](void* p) {
        delete static_cast<S*>(p);
      }),
      std::type_index(typeid(S))});
  return *raw;
}

template<typename C>
bool StateStore::canReuseBody(ComponentKey const& key, C const& value,
                              LayoutConstraints const& constraints) const {
  if (forceFullRebuild_ || activeDirtyComposites_.count(key) != 0) {
    return false;
  }
  auto const it = states_.find(key);
  if (it == states_.end()) {
    return false;
  }
  ComponentState const& state = it->second;
  if (!state.lastBody || state.lastBodyEpoch == buildEpoch_ || state.reusableConstraints.empty()) {
    return false;
  }
  bool constraintsMatched = false;
  for (LayoutConstraints const& recorded : state.reusableConstraints) {
    if (constraintsEqual(recorded, constraints)) {
      constraintsMatched = true;
      break;
    }
  }
  if (!constraintsMatched) {
    return false;
  }
  if (currentCompositePathStable()) {
    return true;
  }
  if (!state.valueSnapshot.value || state.valueSnapshot.type != std::type_index(typeid(C)) ||
      !state.valueSnapshot.equals) {
    return false;
  }
  return state.valueSnapshot.equals(state.valueSnapshot.value.get(), &value);
}

} // namespace flux
