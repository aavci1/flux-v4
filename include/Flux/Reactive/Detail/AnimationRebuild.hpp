#pragma once

/// \file Flux/Reactive/Detail/AnimationRebuild.hpp
///
/// Part of the Flux public API.


namespace flux::detail {

/// Schedules a UI rebuild when an `Animation` value changes (set or tick). Implemented in a .cpp that
/// includes `Application.hpp` so `Animation.hpp` does not create an include cycle with `Application` →
/// `Window` → `Element` → … → `Hooks` → `Animation.hpp`.
void scheduleReactiveRebuildAfterAnimationChange();

} // namespace flux::detail
