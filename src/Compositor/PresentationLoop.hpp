#pragma once

#include "Compositor/CompositorPresentation.hpp"
#include "Compositor/Wayland/WaylandTypes.hpp"

#include <Flux/Platform/Linux/KmsOutput.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "presentation-time-server-protocol.h"

namespace flux::compositor::presentation {

[[nodiscard]] PresentationTiming presentationTimingFromVblank(platform::KmsOutput::VblankTiming const& vblank,
                                                            std::uint32_t refreshMilliHz,
                                                            std::uint64_t fallbackSequence);

void printOutputs(std::vector<flux::platform::KmsOutput> const& outputs);
[[nodiscard]] std::optional<std::size_t> selectOutputIndex(std::vector<flux::platform::KmsOutput> const& outputs,
                                                           std::optional<std::string> const& selector);

} // namespace flux::compositor::presentation
