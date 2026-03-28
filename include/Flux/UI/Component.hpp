#pragma once

#include <utility>

namespace flux {

/// A type is composite if `body()` is callable on `const T` (typical root / description types) or on
/// non-`const T` (registry-backed components that update signals from `body()`).
template<typename T>
concept CompositeComponent =
    requires { std::declval<T const&>().body(); } || requires { std::declval<T&>().body(); };

template<typename T>
concept LeafComponent = !CompositeComponent<T>;

template<typename T>
concept Component = true;

} // namespace flux
