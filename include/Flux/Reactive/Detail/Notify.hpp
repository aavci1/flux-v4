#pragma once

/// \file Flux/Reactive/Detail/Notify.hpp
///
/// Part of the Flux public API.


#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace flux::detail {

/// Runs observer callbacks with snapshot + deferred nested notifications (see Signal.cpp).
void notifyObserverList(std::vector<std::pair<std::uint64_t, std::function<void()>>>& observers);

} // namespace flux::detail
