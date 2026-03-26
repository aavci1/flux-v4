#pragma once

#include <memory>

#include <Flux/Core/Window.hpp>

#include "PlatformWindow.hpp"

namespace flux::detail {

std::unique_ptr<PlatformWindow> createPlatformWindow(const WindowConfig& config);

} // namespace flux::detail
