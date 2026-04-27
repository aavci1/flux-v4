#pragma once

/// \file Flux/Reactive/Reactive.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Easing.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/Reactive/Animation.hpp>
#include <Flux/Reactive/Computed.hpp>
#include <Flux/Reactive/Effect.hpp>
#include <Flux/Reactive/Scope.hpp>
#include <Flux/Reactive/Signal.hpp>
#include <Flux/Reactive/Untrack.hpp>

namespace flux {

template<typename T>
using Signal = Reactive::Signal<T>;

template<typename T>
using Computed = Reactive::Computed<T>;

using Effect = Reactive::Effect;
using Scope = Reactive::Scope;

using Reactive::makeComputed;
using Reactive::onCleanup;
using Reactive::untrack;
using Reactive::withOwner;

} // namespace flux
