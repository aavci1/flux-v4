#pragma once

namespace flux::detail {

/// Schedules a UI rebuild when an `Animated` value changes (set or tick). Implemented in a .cpp that
/// includes `Application.hpp` so `Animated.hpp` does not create an include cycle with `Application` →
/// `Window` → `Element` → … → `Hooks` → `Animated.hpp`.
void scheduleReactiveRebuildAfterAnimatedChange();

} // namespace flux::detail
