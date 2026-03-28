#pragma once

namespace flux::detail {

template<typename T>
constexpr bool equalityComparableV = requires(T const& a, T const& b) {
  { a == b } -> std::convertible_to<bool>;
};

} // namespace flux::detail
