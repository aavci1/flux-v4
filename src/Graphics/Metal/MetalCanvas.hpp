#pragma once

#include <Flux/Graphics/Canvas.hpp>

#include <memory>

namespace flux {

class Window;

/// Creates the Metal-backed canvas for a window (macOS only).
std::unique_ptr<Canvas> createMetalCanvas(Window* window, void* caMetalLayer, unsigned int handle);

} // namespace flux
