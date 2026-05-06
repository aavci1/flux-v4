#pragma once

#include <Flux/Graphics/Canvas.hpp>

#include <memory>

struct wl_display;
struct wl_surface;

namespace flux {

class TextSystem;

std::unique_ptr<Canvas> createVulkanCanvas(wl_display* display, wl_surface* surface,
                                           unsigned int handle, TextSystem& textSystem);

} // namespace flux
