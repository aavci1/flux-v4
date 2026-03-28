#pragma once

namespace flux {

template<typename T>
concept CompositeComponent = requires(T const& t) {
  { t.body() };
};

template<typename T>
concept LeafComponent = !CompositeComponent<T>;

template<typename T>
concept Component = true;

} // namespace flux
