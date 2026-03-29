#pragma once

namespace flux::detail {

/// Implemented in `Application.mm` so `Signal.hpp` does not include `Application.hpp` (breaks the
/// `Application` → `Window` → … → `Hooks` → `Signal` include cycle during `Application` construction).
bool signalBridgeApplicationHasInstance();
void signalBridgeMarkReactiveDirty();

} // namespace flux::detail
