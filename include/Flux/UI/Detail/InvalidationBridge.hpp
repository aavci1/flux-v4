#pragma once

#include <Flux/UI/ComponentKey.hpp>
#include <Flux/UI/Invalidation.hpp>

#include <functional>

namespace flux {

class Runtime;

namespace detail {

Runtime* currentRuntimeForInvalidation() noexcept;
std::function<void()> makeInvalidationCallback(Runtime* runtime, ComponentKey key, InvalidationKind kind);

} // namespace detail

} // namespace flux
