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
#include <utility>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace flux {

namespace detail {

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

inline bool useFocus() { return false; }
inline bool useKeyboardFocus() { return false; }
inline bool useHover() { return false; }
inline bool usePress() { return false; }

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
