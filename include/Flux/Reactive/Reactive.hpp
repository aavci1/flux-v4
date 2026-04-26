#pragma once

/// \file Flux/Reactive/Reactive.hpp
///
/// Part of the Flux public API.


#include <Flux/Reactive/Observer.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Easing.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/Reactive/AnimationClock.hpp>
#include <Flux/Reactive/Animation.hpp>
#include <Flux/Reactive2/Computed.hpp>
#include <Flux/Reactive2/Effect.hpp>
#include <Flux/Reactive2/Scope.hpp>
#include <Flux/Reactive2/Signal.hpp>
#include <Flux/Reactive2/Untrack.hpp>

namespace flux {

template<typename T>
using Signal = Reactive2::Signal<T>;

template<typename T>
using Computed = Reactive2::Computed<T>;

using Effect = Reactive2::Effect;
using Scope = Reactive2::Scope;

using Reactive2::makeComputed;
using Reactive2::onCleanup;
using Reactive2::untrack;
using Reactive2::withOwner;

} // namespace flux
