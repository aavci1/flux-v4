#pragma once

/// \file Flux/UI/Hooks.hpp
///
/// Scope-owned v5 hooks and environment accessors.

#include <Flux/Reactive/Animation.hpp>
#include <Flux/Reactive/Computed.hpp>
#include <Flux/Reactive/Effect.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Core/ComponentKey.hpp>
#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Theme.hpp>

#include <cmath>
#include <cassert>
#include <cstdint>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace flux {

namespace detail {

struct InteractionSignalBundle {
  Reactive::Signal<bool> hover;
  Reactive::Signal<bool> press;
  Reactive::Signal<bool> focus;
  Reactive::Signal<bool> keyboardFocus;
};

struct OwnerInteractionSignals {
  InteractionSignalBundle signals;
  bool cleanupRegistered = false;
};

#ifndef NDEBUG
struct UseStateDebugKey {
  char const* file = nullptr;
  std::uint_least32_t line = 0;
  std::uint_least32_t column = 0;

  bool operator==(UseStateDebugKey const&) const = default;
};

struct UseStateDebugKeyHash {
  std::size_t operator()(UseStateDebugKey const& key) const noexcept {
    std::size_t seed = std::hash<char const*>{}(key.file);
    seed ^= std::hash<std::uint_least32_t>{}(key.line) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    seed ^= std::hash<std::uint_least32_t>{}(key.column) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
    return seed;
  }
};

struct OwnerUseStateDebugInfo {
  std::unordered_set<UseStateDebugKey, UseStateDebugKeyHash> callSites;
  bool cleanupRegistered = false;
};
#endif

inline std::unordered_map<Reactive::detail::ScopeState*, OwnerInteractionSignals>&
ownerInteractionSignals() {
  static thread_local std::unordered_map<Reactive::detail::ScopeState*, OwnerInteractionSignals> signals;
  return signals;
}

#ifndef NDEBUG
inline std::unordered_map<Reactive::detail::ScopeState*, OwnerUseStateDebugInfo>&
ownerUseStateDebugInfo() {
  static thread_local std::unordered_map<Reactive::detail::ScopeState*, OwnerUseStateDebugInfo> info;
  return info;
}
#endif

inline std::vector<InteractionSignalBundle const*>& interactionSignalMountStack() {
  static thread_local std::vector<InteractionSignalBundle const*> stack;
  return stack;
}

inline std::vector<ComponentKey const*>& interactionScopeKeyMountStack() {
  static thread_local std::vector<ComponentKey const*> stack;
  return stack;
}

inline InteractionSignalBundle const* currentInteractionSignals() {
  auto& stack = interactionSignalMountStack();
  return stack.empty() ? nullptr : stack.back();
}

inline ComponentKey const* currentInteractionScopeKey() {
  auto& stack = interactionScopeKeyMountStack();
  return stack.empty() ? nullptr : stack.back();
}

class HookInteractionSignalScope {
public:
  explicit HookInteractionSignalScope(Reactive::Scope& owner)
      : scopeKey_(ComponentKey::fromScope(owner.state())) {
    auto const it = ownerInteractionSignals().find(owner.state());
    if (it != ownerInteractionSignals().end()) {
      signals_ = &it->second.signals;
    }
    interactionSignalMountStack().push_back(signals_);
    interactionScopeKeyMountStack().push_back(&scopeKey_);
  }

  HookInteractionSignalScope(HookInteractionSignalScope const&) = delete;
  HookInteractionSignalScope& operator=(HookInteractionSignalScope const&) = delete;

  ~HookInteractionSignalScope() {
    auto& signalStack = interactionSignalMountStack();
    if (!signalStack.empty()) {
      signalStack.pop_back();
    }
    auto& keyStack = interactionScopeKeyMountStack();
    if (!keyStack.empty()) {
      keyStack.pop_back();
    }
  }

private:
  ComponentKey scopeKey_;
  InteractionSignalBundle const* signals_ = nullptr;
};

inline std::vector<LayoutConstraints>& hookLayoutConstraintStack() {
  static thread_local std::vector<LayoutConstraints> stack;
  return stack;
}

class HookLayoutScope {
public:
  explicit HookLayoutScope(LayoutConstraints constraints) {
    hookLayoutConstraintStack().push_back(constraints);
  }

  HookLayoutScope(HookLayoutScope const&) = delete;
  HookLayoutScope& operator=(HookLayoutScope const&) = delete;

  ~HookLayoutScope() {
    auto& stack = hookLayoutConstraintStack();
    if (!stack.empty()) {
      stack.pop_back();
    }
  }
};

inline LayoutConstraints const* currentHookLayoutConstraints() {
  auto& stack = hookLayoutConstraintStack();
  return stack.empty() ? nullptr : &stack.back();
}

enum class InteractionSignalKind {
  Hover,
  Press,
  Focus,
  KeyboardFocus,
};

inline Reactive::Signal<bool>& signalSlot(InteractionSignalBundle& signals,
                                          InteractionSignalKind kind) {
  switch (kind) {
  case InteractionSignalKind::Hover:
    return signals.hover;
  case InteractionSignalKind::Press:
    return signals.press;
  case InteractionSignalKind::Focus:
    return signals.focus;
  case InteractionSignalKind::KeyboardFocus:
    return signals.keyboardFocus;
  }
  return signals.hover;
}

inline Reactive::Signal<bool> useInteractionSignal(InteractionSignalKind kind) {
  Reactive::Signal<bool> signal{false};
  Reactive::detail::ScopeState* owner = Reactive::detail::sCurrentOwner;
  if (!owner) {
    return signal;
  }

  auto& entry = ownerInteractionSignals()[owner];
  signalSlot(entry.signals, kind) = signal;
  if (!entry.cleanupRegistered) {
    entry.cleanupRegistered = true;
    Reactive::onCleanup([owner] {
      ownerInteractionSignals().erase(owner);
    });
  }
  return signal;
}

#ifndef NDEBUG
inline void debugRegisterUseState(std::source_location location) {
  Reactive::detail::ScopeState* owner = Reactive::detail::sCurrentOwner;
  if (!owner) {
    return;
  }

  auto& entry = ownerUseStateDebugInfo()[owner];
  UseStateDebugKey const key{
      .file = location.file_name(),
      .line = location.line(),
      .column = location.column(),
  };
  bool const inserted = entry.callSites.insert(key).second;
  assert(inserted && "useState call site was evaluated more than once in the same mount scope");
  if (!entry.cleanupRegistered) {
    entry.cleanupRegistered = true;
    Reactive::onCleanup([owner] {
      ownerUseStateDebugInfo().erase(owner);
    });
  }
}
#endif

} // namespace detail

template<typename T>
T const& environmentFallback() {
  static T fallback{};
  return fallback;
}

template<>
inline Theme const& environmentFallback<Theme>() {
  static Theme const fallback = Theme::light();
  return fallback;
}

template<typename T>
Reactive::Signal<T> useEnvironment() {
  if (auto const* signal = EnvironmentStack::current().findSignal<T>()) {
    return *signal;
  }
  if (T const* value = EnvironmentStack::current().find<T>()) {
    return Reactive::Signal<T>{*value};
  }
  return Reactive::Signal<T>{environmentFallback<T>()};
}

/// Each `useState` call allocates a fresh `Signal<T>` owned by the current reactive scope.
/// The v5 retained UI model guarantees `body()` runs exactly once for a mount cycle; therefore
/// each `useState` call site allocates one signal for that cycle. Re-entering the same body and
/// evaluating the same call site twice is a framework error and is asserted in debug builds.
template<typename T>
Reactive::Signal<T> useState(T initial = T{},
                             std::source_location location = std::source_location::current()) {
#ifndef NDEBUG
  detail::debugRegisterUseState(location);
#else
  (void)location;
#endif
  return Reactive::Signal<T>(std::move(initial));
}

template<typename Fn>
auto useComputed(Fn&& fn) {
  return Reactive::makeComputed(std::forward<Fn>(fn));
}

template<typename Fn>
void useEffect(Fn&& fn) {
  Reactive::Effect{std::forward<Fn>(fn)};
}

template<Interpolatable T>
Animation<T> useAnimation(T initial = T{}) {
  return Animation<T>(std::move(initial));
}

template<Interpolatable T>
Animation<T> useAnimation(T initial, AnimationOptions options) {
  Animation<T> animation{std::move(initial)};
  animation.play(animation.get(), std::move(options));
  return animation;
}

template<typename Fn>
void useAnimationFrame(Fn&& callback) {
  ObserverHandle const handle =
      AnimationClock::instance().subscribe(Reactive::SmallFn<void(AnimationTick const&)>{std::forward<Fn>(callback)});
  Reactive::onCleanup([handle] {
    AnimationClock::instance().unsubscribe(handle);
  });
}

inline Reactive::Signal<bool> useFocus() {
  return detail::useInteractionSignal(detail::InteractionSignalKind::Focus);
}

inline Reactive::Signal<bool> useKeyboardFocus() {
  return detail::useInteractionSignal(detail::InteractionSignalKind::KeyboardFocus);
}

inline Reactive::Signal<bool> useHover() {
  return detail::useInteractionSignal(detail::InteractionSignalKind::Hover);
}

inline Reactive::Signal<bool> usePress() {
  return detail::useInteractionSignal(detail::InteractionSignalKind::Press);
}

inline LayoutConstraints const* useLayoutConstraints() {
  return detail::currentHookLayoutConstraints();
}

inline Rect useBounds() {
  LayoutConstraints const* constraints = useLayoutConstraints();
  if (!constraints) {
    return {};
  }

  Rect bounds{};
  if (std::isfinite(constraints->maxWidth) && constraints->maxWidth > 0.f) {
    bounds.width = constraints->maxWidth;
  }
  if (std::isfinite(constraints->maxHeight) && constraints->maxHeight > 0.f) {
    bounds.height = constraints->maxHeight;
  }
  return bounds;
}

inline void useViewAction(std::string const& name, std::function<void()> handler,
                          std::function<bool()> isEnabled = {}) {
  Runtime* runtime = Runtime::current();
  if (!runtime) {
    return;
  }
  Reactive::detail::ScopeState* scope = Reactive::detail::sCurrentOwner;
  ComponentKey const key = scope ? ComponentKey::fromScope(scope) : ComponentKey{};
  ActionId const id = runtime->actionRegistry().registerViewClaim(
      key, name, std::move(handler), std::move(isEnabled));
  Reactive::onCleanup([runtime, id] {
    runtime->actionRegistry().unregister(id);
  });
}

inline void useWindowAction(std::string const& name, std::function<void()> handler,
                            std::function<bool()> isEnabled = {}) {
  Runtime* runtime = Runtime::current();
  if (!runtime) {
    return;
  }
  ActionId const id = runtime->actionRegistry().registerWindowAction(
      name, std::move(handler), std::move(isEnabled));
  Reactive::onCleanup([runtime, id] {
    runtime->actionRegistry().unregister(id);
  });
}

} // namespace flux
