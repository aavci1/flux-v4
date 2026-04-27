#pragma once

/// \file Flux/UI/Hooks.hpp
///
/// Scope-owned v5 hooks and environment accessors.

#include <Flux/Reactive/Animation.hpp>
#include <Flux/Reactive/Computed.hpp>
#include <Flux/Reactive/Effect.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/LayoutEngine.hpp>
#include <Flux/UI/Theme.hpp>

#include <cmath>
#include <functional>
#include <unordered_map>
#include <optional>
#include <string>
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

inline std::unordered_map<Reactive::detail::ScopeState*, OwnerInteractionSignals>&
ownerInteractionSignals() {
  static thread_local std::unordered_map<Reactive::detail::ScopeState*, OwnerInteractionSignals> signals;
  return signals;
}

inline std::vector<InteractionSignalBundle const*>& interactionSignalMountStack() {
  static thread_local std::vector<InteractionSignalBundle const*> stack;
  return stack;
}

inline InteractionSignalBundle const* currentInteractionSignals() {
  auto& stack = interactionSignalMountStack();
  return stack.empty() ? nullptr : stack.back();
}

class HookInteractionSignalScope {
public:
  explicit HookInteractionSignalScope(Reactive::Scope& owner) {
    auto const it = ownerInteractionSignals().find(owner.state());
    if (it != ownerInteractionSignals().end()) {
      signals_ = &it->second.signals;
    }
    interactionSignalMountStack().push_back(signals_);
  }

  HookInteractionSignalScope(HookInteractionSignalScope const&) = delete;
  HookInteractionSignalScope& operator=(HookInteractionSignalScope const&) = delete;

  ~HookInteractionSignalScope() {
    auto& stack = interactionSignalMountStack();
    if (!stack.empty()) {
      stack.pop_back();
    }
  }

private:
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
EnvironmentValue<T> useEnvironment() {
  if (auto const* signal = EnvironmentStack::current().findSignal<T>()) {
    return EnvironmentValue<T>{*signal};
  }
  if (T const* value = EnvironmentStack::current().find<T>()) {
    return EnvironmentValue<T>{*value};
  }
  return EnvironmentValue<T>{environmentFallback<T>()};
}

template<typename T>
struct State {
  Reactive::Signal<T> signal{};

  State() : signal(T{}) {}
  explicit State(T initial) : signal(std::move(initial)) {}

  T const& get() const { return signal.get(); }
  T const& peek() const { return signal.peek(); }
  T const& operator()() const { return signal.get(); }
  T const& operator*() const { return signal.get(); }
  void set(T value) const { signal.set(std::move(value)); }
  void setSilently(T value) const { signal.set(std::move(value)); }

  State const& operator=(T value) const {
    signal.set(std::move(value));
    return *this;
  }

  explicit operator bool() const requires std::same_as<T, bool> {
    return signal.get();
  }

  bool operator==(State const&) const noexcept { return false; }
};

template<typename T>
State<T> useState(T initial = T{}) {
  return State<T>(std::move(initial));
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
  (void)name;
  (void)handler;
  (void)isEnabled;
}

inline void useWindowAction(std::string const& name, std::function<void()> handler,
                            std::function<bool()> isEnabled = {}) {
  (void)name;
  (void)handler;
  (void)isEnabled;
}

} // namespace flux
