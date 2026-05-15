#pragma once

#include "Graphics/Vulkan/VulkanCanvasTypes.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T *;

namespace flux {

/// Per-frame CPU-side Vulkan display list, detachable from a canvas and replayable
/// into later frames.
struct VulkanFrameRecorder {
  std::vector<DrawOp> ops;
  std::vector<QuadInstance> quads;
  std::vector<RectInstance> rects;
  std::vector<VulkanPathVertex> pathVerts;

  mutable VkBuffer preparedQuadBuffer = VK_NULL_HANDLE;
  mutable VmaAllocation preparedQuadAllocation = VK_NULL_HANDLE;
  mutable VkDeviceSize preparedQuadCapacity = 0;

  mutable VkBuffer preparedRectBuffer = VK_NULL_HANDLE;
  mutable VmaAllocation preparedRectAllocation = VK_NULL_HANDLE;
  mutable VkDeviceSize preparedRectCapacity = 0;

  mutable VkBuffer preparedPathVertexBuffer = VK_NULL_HANDLE;
  mutable VmaAllocation preparedPathVertexAllocation = VK_NULL_HANDLE;
  mutable VkDeviceSize preparedPathVertexCapacity = 0;

  mutable VmaAllocator allocator = VK_NULL_HANDLE;

  Rect rootClip{};
  std::uint64_t glyphAtlasGeneration = 0;

  VulkanFrameRecorder() = default;
  ~VulkanFrameRecorder();
  VulkanFrameRecorder(VulkanFrameRecorder const &) = delete;
  VulkanFrameRecorder &operator=(VulkanFrameRecorder const &) = delete;
  VulkanFrameRecorder(VulkanFrameRecorder &&other) noexcept;
  VulkanFrameRecorder &operator=(VulkanFrameRecorder &&other) noexcept;

  void clear();
};

} // namespace flux
