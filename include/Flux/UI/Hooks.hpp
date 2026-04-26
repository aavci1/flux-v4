#pragma once

/// \file Flux/UI/Hooks.hpp
///
/// Stage 4 keeps hooks intentionally narrow: environment reads are available so
/// static roots can mount, while the state/effect/input hooks are rebuilt in
/// Stage 5 with Scope-owned v5 semantics.

#include <Flux/Reactive2/Signal.hpp>
#include <Flux/UI/Environment.hpp>
#include <Flux/UI/Theme.hpp>

#include <utility>

namespace flux {

template<typename T>
T const& useEnvironment() {
  if (T const* value = EnvironmentStack::current().find<T>()) {
    return *value;
  }
  static T fallback{};
  return fallback;
}

template<>
inline Theme const& useEnvironment<Theme>() {
  if (Theme const* theme = EnvironmentStack::current().find<Theme>()) {
    return *theme;
  }
  static Theme const fallback = Theme::light();
  return fallback;
}

template<typename T>
struct State {
  Reactive2::Signal<T> signal{};

  State() = default;
  explicit State(T initial) : signal(std::move(initial)) {}

  T const& get() const { return signal.get(); }
  T const& operator()() const { return signal.get(); }
  void set(T value) const { signal.set(std::move(value)); }

  bool operator==(State const&) const noexcept { return false; }
};

template<typename T>
State<T> useState(T initial = T{}) {
  return State<T>(std::move(initial));
}

} // namespace flux
