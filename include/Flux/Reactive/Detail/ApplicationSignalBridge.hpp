#pragma once

/// \file Flux/Reactive/Detail/ApplicationSignalBridge.hpp
///
/// Part of the Flux public API.


namespace flux::detail {

/// Implemented in `Application.mm` so `Signal.hpp` does not include `Application.hpp` (breaks the
/// `Application` → `Window` → … → `Hooks` → `Signal` include cycle during `Application` construction).
bool signalBridgeApplicationHasInstance();
void signalBridgeMarkSlotDirty(void* slot);

} // namespace flux::detail
