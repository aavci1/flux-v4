#pragma once

/// \file Flux/UI/StateStore.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/ComponentKey.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Detail/ElementModifiers.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/Reactive/Observer.hpp>
#include <Flux/Reactive/Detail/TypeTraits.hpp>

#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flux {

class Element;
namespace scenegraph {
struct InteractionData;
}

/// One heap-allocated state value (a Signal<T>, Animation<T>, or any type).
struct StateSlot {
  std::unique_ptr<void, void (*)(void*)> value{nullptr, nullptr};
  std::type_index type{typeid(void)};
  Observable* observable = nullptr;
  ObserverHandle ownerHandle{};
};

struct ComponentSubscription {
  Observable* observable = nullptr;
  ObserverHandle handle{};
};

struct EnvironmentValueSnapshot {
  std::unique_ptr<void, void (*)(void*)> value{nullptr, nullptr};
  bool (*equalsCurrent)(void const*, EnvironmentStack const&) = nullptr;
  std::type_index type{typeid(void)};
};

struct ComponentBuildSnapshot {
  LayoutConstraints constraints{};
  LayoutHints hints{};
  Point origin{};
  Point rootPosition{};
  Size assignedSize{};
  bool hasAssignedWidth = false;
  bool hasAssignedHeight = false;
};

/// State bucket for one component instance.
struct ComponentState {
  std::deque<StateSlot> slots;
  std::size_t cursor = 0; // reset to 0 at the start of each build pass
  std::type_index componentType{typeid(void)};
  std::uint64_t lastVisitedEpoch = 0;
  std::unique_ptr<void, void (*)(void*)> lastBody{nullptr, nullptr};
  std::uint64_t lastBodyEpoch = 0;
  bool lastBodyStructurallyStable = false;
  std::optional<LayoutConstraints> lastBodyConstraints;
  std::vector<std::pair<LayoutConstraints, Size>> reusableMeasures;
  std::vector<EnvironmentValueSnapshot> environmentDependencies;
  std::vector<EnvironmentValueSnapshot> pendingEnvironmentDependencies;
  std::vector<ComponentSubscription> subscriptions;
  std::unique_ptr<void, void (*)(void*)> lastSceneElement{nullptr, nullptr};
  std::optional<ComponentBuildSnapshot> lastBuildSnapshot;
};

struct InteractionCallbackCells {
  std::shared_ptr<std::function<void(Point)>> onPointerDown{};
  std::shared_ptr<std::function<void(Point)>> onPointerUp{};
  std::shared_ptr<std::function<void(Point)>> onPointerMove{};
  std::shared_ptr<std::function<void(Vec2)>> onScroll{};
  std::shared_ptr<std::function<void(KeyCode, Modifiers)>> onKeyDown{};
  std::shared_ptr<std::function<void(KeyCode, Modifiers)>> onKeyUp{};
  std::shared_ptr<std::function<void(std::string const&)>> onTextInput{};
  std::shared_ptr<std::function<void()>> onTap{};
  std::uint64_t lastVisitedEpoch = 0;
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
  void pushCompositeElementModifiers(detail::ElementModifiers const* m);
  void popCompositeElementModifiers();
  detail::ElementModifiers const* currentCompositeElementModifiers() const noexcept;

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
  [[nodiscard]] std::vector<ComponentKey> pendingDirtyComponents() const;
  [[nodiscard]] bool shouldForceFullRebuild() const noexcept { return forceFullRebuild_; }
  [[nodiscard]] bool isComponentDirty(ComponentKey const& key) const;
  [[nodiscard]] bool hasDirtyDescendant(ComponentKey const& key) const;
  void markComponentsOutsideSubtreeVisited(ComponentKey const& key);
  /// Marks the component at \p key and every descendant component state as visited for this rebuild.
  void markRetainedSubtreeVisited(ComponentKey const& key);
  void markRetainedSubtreeVisited(ComponentState& state);

  [[nodiscard]] ComponentState const* findComponentState(ComponentKey const& key) const;
  [[nodiscard]] ComponentState* findComponentState(ComponentKey const& key);

  [[nodiscard]] bool hasBodyForCurrentRebuild(ComponentKey const& key,
                                              LayoutConstraints const& constraints) const;
  [[nodiscard]] bool bodyStructurallyStable(ComponentKey const& key) const;
  Element* cachedBody(ComponentKey const& key);
  Element const* cachedBody(ComponentKey const& key) const;
  void recordBodyStability(ComponentKey const& key, bool stable);
  Element const* sceneElement(ComponentKey const& key) const;
  void discardCurrentRebuildBody(ComponentKey const& key);
  std::optional<ComponentBuildSnapshot> buildSnapshot(ComponentKey const& key) const;
  void recordBuildSnapshot(ComponentKey const& key, LayoutConstraints const& constraints,
                           LayoutHints const& hints, Point origin, Size assignedSize,
                           bool hasAssignedWidth, bool hasAssignedHeight,
                           Point rootPosition = {});
  void recordSceneElement(ComponentKey const& key, Element const& element);
  [[nodiscard]] bool modifierLayersStructurallyStable(
      ComponentKey const& key, std::span<detail::ElementModifiers const> layers) const;
  void recordModifierLayers(ComponentKey const& key,
                            std::span<detail::ElementModifiers const> layers);
  void recordInteraction(ComponentKey const& key, detail::ElementModifiers const& modifiers);
  [[nodiscard]] std::unique_ptr<scenegraph::InteractionData>
  makeInteractionData(ComponentKey const& key, detail::ElementModifiers const& modifiers);
  [[nodiscard]] bool hasInteractionDescendant(ComponentKey const& key) const;
  std::optional<Size> cachedMeasure(ComponentKey const& key, LayoutConstraints const& constraints) const;
  void recordMeasure(ComponentKey const& key, LayoutConstraints const& constraints, Size size);
  void recordCompositeBodyResolve(bool comparedPreviousBody, bool structurallyStable,
                                  bool legacyPredicateWouldHaveMatched);

  template<typename C>
  Element& commitBody(ComponentKey const& key, C const& value, LayoutConstraints const& constraints,
                      std::unique_ptr<Element> body, std::vector<Observable*> deps);

  template<typename T>
  void recordEnvironmentDependency(T const& value);

  /// When building an overlay subtree, set to that overlay's id value (for `useFocus` / `useHover`).
  void setOverlayScope(std::optional<std::uint64_t> overlayIdValue);
  std::optional<std::uint64_t> overlayScope() const;

  /// Thread-local pointer to the active StateStore during a build pass.
  /// Set by Runtime::rebuild; accessed by useState/useAnimation free functions.
  static StateStore* current() noexcept;
  static void setCurrent(StateStore* s) noexcept;

private:
  std::unordered_map<ComponentKey, ComponentState, ComponentKeyHash> states_;
  std::unordered_map<ComponentKey, InteractionCallbackCells, ComponentKeyHash> interactions_;
  std::unordered_map<ComponentKey, std::vector<detail::ElementModifiers>, ComponentKeyHash>
      modifierLayers_;

  // Stack of active component keys (depth > 1 when body() calls a helper
  // that returns an Element containing further composites — rare but valid).
  std::vector<ComponentKey const*> activeStack_;
  std::vector<ComponentState*> activeStateStack_;

  std::optional<std::uint64_t> overlayScope_{};

  std::vector<LayoutConstraints> compositeConstraintStack_{};
  std::vector<detail::ElementModifiers const*> compositeElementModifierStack_{};
  std::unordered_set<ComponentKey, ComponentKeyHash> pendingDirtyComposites_{};
  std::unordered_set<ComponentKey, ComponentKeyHash> activeDirtyComposites_{};
  std::unordered_set<ComponentKey, ComponentKeyHash> activeDirtyAncestorKeys_{};
  bool forceFullRebuild_ = true;
  std::uint64_t buildEpoch_ = 0;

  static thread_local StateStore* sCurrent;

  static bool constraintsEqual(LayoutConstraints const& a, LayoutConstraints const& b) noexcept;
  static bool rectEqual(Rect const& a, Rect const& b) noexcept;
  void clearComponentState(ComponentState& state);
  bool environmentDependenciesMatch(ComponentState const& state) const;

  template<typename T>
  static EnvironmentValueSnapshot makeEnvironmentSnapshot(T const& value);
};

} // namespace flux

// --- template implementation ---

namespace flux {

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
  Observable* slotObservable = nullptr;
  ObserverHandle ownerHandle{};
  if constexpr (std::is_base_of_v<Observable, S>) {
    slotObservable = raw;
    ownerHandle = raw->observeComposite(*this, *activeStack_.back());
  }
  cs.slots.push_back(StateSlot{
      std::unique_ptr<void, void (*)(void*)>(raw, [](void* p) {
        delete static_cast<S*>(p);
      }),
      std::type_index(typeid(S)),
      slotObservable,
      ownerHandle});
  return *raw;
}

template<typename T>
EnvironmentValueSnapshot StateStore::makeEnvironmentSnapshot(T const& value) {
  EnvironmentValueSnapshot snapshot{};
  if constexpr (std::is_copy_constructible_v<T>) {
    T* raw = new T(value);
    snapshot.value = std::unique_ptr<void, void (*)(void*)>(
        raw, [](void* p) { delete static_cast<T*>(p); });
    if constexpr (detail::equalityComparableV<T>) {
      snapshot.equalsCurrent = [](void const* lhs, EnvironmentStack const& environment) {
        if (T const* current = environment.find<T>()) {
          return *static_cast<T const*>(lhs) == *current;
        }
        static T const fallback{};
        return *static_cast<T const*>(lhs) == fallback;
      };
    } else if constexpr (std::is_trivially_copyable_v<T>) {
      snapshot.equalsCurrent = [](void const* lhs, EnvironmentStack const& environment) {
        T const* current = environment.find<T>();
        static T const fallback{};
        T const* rhs = current ? current : &fallback;
        return std::memcmp(lhs, rhs, sizeof(T)) == 0;
      };
    }
    snapshot.type = std::type_index(typeid(T));
  }
  return snapshot;
}

template<typename T>
void StateStore::recordEnvironmentDependency(T const& value) {
  if (activeStateStack_.empty()) {
    return;
  }
  ComponentState& state = *activeStateStack_.back();
  std::type_index const type = std::type_index(typeid(T));
  for (EnvironmentValueSnapshot const& dep : state.pendingEnvironmentDependencies) {
    if (dep.type == type) {
      return;
    }
  }
  state.pendingEnvironmentDependencies.push_back(makeEnvironmentSnapshot(value));
}

} // namespace flux
