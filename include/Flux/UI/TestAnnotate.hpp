#pragma once

/// \file Flux/UI/TestAnnotate.hpp
/// Hooks for `TestTreeAnnotator` during `Element::build` (only active in `--test-mode`).

#include <Flux/Core/Types.hpp>
#include <Flux/UI/BuildContext.hpp>
#include <Flux/UI/TestTreeAnnotator.hpp>

#include <cstdlib>
#include <cxxabi.h>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>

namespace flux::detail {

inline std::string demangledTypeName(char const* mangled) {
  int status = 0;
  char* dem = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
  if (dem) {
    std::string s(dem);
    std::free(dem);
    return s;
  }
  return mangled ? std::string(mangled) : std::string("?");
}

template<typename C>
std::string testTypeNameFor() {
  if constexpr (requires { C::testName; }) {
    return std::string(C::testName);
  }
  return demangledTypeName(typeid(C).name());
}

template<typename C>
std::string testTextOf(C const& v) {
  if constexpr (requires { v.text; }) {
    if constexpr (requires { v.text.empty(); }) {
      if (!v.text.empty()) {
        return std::string(v.text);
      }
    } else if constexpr (requires { { *v.text } -> std::convertible_to<std::string const&>; }) {
      std::string const& s = *v.text;
      if (!s.empty()) {
        return s;
      }
    }
  }
  if constexpr (requires { v.label; }) {
    if constexpr (requires { v.label.empty(); }) {
      if (!v.label.empty()) {
        return std::string(v.label);
      }
    } else if constexpr (requires { { *v.label } -> std::convertible_to<std::string const&>; }) {
      std::string const& s = *v.label;
      if (!s.empty()) {
        return s;
      }
    }
  }
  return {};
}

template<typename C>
std::string testValueOf(C const& v) {
  if constexpr (requires { v.value; }) {
    using Val = std::decay_t<decltype(v.value)>;
    if constexpr (std::is_same_v<Val, bool>) {
      return v.value ? std::string("true") : std::string("false");
    } else if constexpr (std::is_arithmetic_v<Val>) {
      std::ostringstream oss;
      oss << v.value;
      return oss.str();
    } else {
      return {};
    }
  }
  return {};
}

template<typename C>
std::string testFocusKeyOf(C const& v) {
  if constexpr (requires { v.testFocusKey; }) {
    return v.testFocusKey;
  }
  return {};
}

template<typename C>
bool testFocusableOf(C const& v) {
  if constexpr (requires { v.focusable; }) {
    return v.focusable;
  }
  return false;
}

template<typename C>
bool testInteractiveOf(C const& v) {
  if constexpr (requires { v.onTap; }) {
    if (v.onTap) {
      return true;
    }
  }
  if constexpr (requires { v.onPointerDown; }) {
    if (v.onPointerDown) {
      return true;
    }
  }
  return testFocusableOf(v);
}

template<typename C>
void annotateCompositeEnter(BuildContext& ctx, C const& value, ComponentKey const& key) {
  if (auto* a = ctx.testAnnotator()) {
    a->pushComposite(testTypeNameFor<C>(), key, testTextOf(value), testValueOf(value), testFocusKeyOf(value),
                     testInteractiveOf(value), testFocusableOf(value));
  }
}

inline void annotateCompositeExit(BuildContext& ctx) {
  if (auto* a = ctx.testAnnotator()) {
    a->popComposite();
  }
}

template<typename C>
void annotateLeaf(BuildContext& ctx, C const& value, ComponentKey const& key, Rect const& bounds) {
  if (auto* a = ctx.testAnnotator()) {
    a->addLeaf(testTypeNameFor<C>(), key, bounds, testTextOf(value), testValueOf(value), testFocusKeyOf(value),
               testInteractiveOf(value), testFocusableOf(value));
  }
}

} // namespace flux::detail
