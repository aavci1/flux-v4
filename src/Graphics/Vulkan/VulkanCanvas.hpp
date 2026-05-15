#pragma once

#include <Flux/Graphics/Canvas.hpp>

#include <filesystem>
#include <memory>
#include <span>

#include <vulkan/vulkan.h>

namespace flux {

class TextSystem;
struct VulkanRenderTargetSpec;

void configureVulkanCanvasRuntime(std::span<char const* const> requiredInstanceExtensions,
                                  std::filesystem::path cacheDir);
VkInstance ensureSharedVulkanInstance();
std::unique_ptr<Canvas> createVulkanCanvas(VkSurfaceKHR surface, unsigned int handle, TextSystem& textSystem);
std::unique_ptr<Canvas> createVulkanRenderTargetCanvas(VulkanRenderTargetSpec const& spec,
                                                       TextSystem& textSystem);

} // namespace flux
