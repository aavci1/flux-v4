#pragma once

/// \file Flux/UI/Environment.hpp
///
/// Part of the Flux public API.


#include <Flux/UI/Detail/EnvironmentSlot.hpp>

#include <concepts>
#include <typeinfo>

namespace flux {

template<typename Tag>
struct EnvironmentKey;

#define FLUX_DEFINE_ENVIRONMENT_KEY(KeyTag, ValueT, DefaultExpr)                         \
  struct KeyTag {};                                                                       \
  template<>                                                                              \
  struct EnvironmentKey<KeyTag> {                                                         \
    using Value = ValueT;                                                                 \
    static_assert(std::copy_constructible<Value>,                                         \
                  "Environment key values must be copy-constructible.");                  \
    static_assert(std::equality_comparable<Value>,                                        \
                  "Environment key values must define operator==.");                      \
    static Value defaultValue() { return (DefaultExpr); }                                 \
    static ::flux::detail::EnvironmentSlot const& slot() {                                \
      static ::flux::detail::EnvironmentSlot s{                                           \
          ::flux::detail::allocateEnvironmentSlot(typeid(KeyTag))};                       \
      return s;                                                                           \
    }                                                                                     \
  }

} // namespace flux
