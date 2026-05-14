#pragma once

#include <memory>

#include <Flux/UI/Window.hpp>

#include "Window.hpp"

namespace flux::platform {

std::unique_ptr<Window> createWindow(WindowConfig const& config);

} // namespace flux::platform
