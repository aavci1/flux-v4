#pragma once

#include <Flux/Graphics/Canvas.hpp>

#include <filesystem>
#include <memory>
#include <span>

#include <vulkan/vulkan.h>

namespace flux {

class TextSystem;

void configureVulkanCanvasRuntime(std::span<char const* const> requiredInstanceExtensions,
                                  std::filesystem::path cacheDir);
VkInstance ensureSharedVulkanInstance();
std::unique_ptr<Canvas> createVulkanCanvas(VkSurfaceKHR surface, unsigned int handle, TextSystem& textSystem);

} // namespace flux
