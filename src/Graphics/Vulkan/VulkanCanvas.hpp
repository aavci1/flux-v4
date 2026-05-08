#pragma once

#include <Flux/Graphics/Canvas.hpp>

#include <memory>

#include <vulkan/vulkan.h>

namespace flux {

class TextSystem;

VkInstance ensureSharedVulkanInstance();
std::unique_ptr<Canvas> createVulkanCanvas(VkSurfaceKHR surface, unsigned int handle, TextSystem& textSystem);

} // namespace flux
