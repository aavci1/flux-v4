#include "Graphics/Vulkan/VulkanFrameRecorder.hpp"

#include "vma/vk_mem_alloc.h"

#include <utility>

namespace flux {

namespace {

void destroyRecorderBuffer(VmaAllocator allocator, VkBuffer &buffer, VmaAllocation &allocation,
                           VkDeviceSize &capacity) noexcept {
  if (allocator && buffer) {
    vmaDestroyBuffer(allocator, buffer, allocation);
  }
  buffer = VK_NULL_HANDLE;
  allocation = VK_NULL_HANDLE;
  capacity = 0;
}

} // namespace

VulkanFrameRecorder::~VulkanFrameRecorder() {
  clear();
}

VulkanFrameRecorder::VulkanFrameRecorder(VulkanFrameRecorder &&other) noexcept
    : ops(std::move(other.ops)),
      quads(std::move(other.quads)),
      rects(std::move(other.rects)),
      pathVerts(std::move(other.pathVerts)),
      preparedQuadBuffer(std::exchange(other.preparedQuadBuffer, VK_NULL_HANDLE)),
      preparedQuadAllocation(std::exchange(other.preparedQuadAllocation, VK_NULL_HANDLE)),
      preparedQuadCapacity(std::exchange(other.preparedQuadCapacity, 0)),
      preparedRectBuffer(std::exchange(other.preparedRectBuffer, VK_NULL_HANDLE)),
      preparedRectAllocation(std::exchange(other.preparedRectAllocation, VK_NULL_HANDLE)),
      preparedRectCapacity(std::exchange(other.preparedRectCapacity, 0)),
      preparedPathVertexBuffer(std::exchange(other.preparedPathVertexBuffer, VK_NULL_HANDLE)),
      preparedPathVertexAllocation(std::exchange(other.preparedPathVertexAllocation, VK_NULL_HANDLE)),
      preparedPathVertexCapacity(std::exchange(other.preparedPathVertexCapacity, 0)),
      allocator(std::exchange(other.allocator, VK_NULL_HANDLE)),
      rootClip(other.rootClip),
      glyphAtlasGeneration(std::exchange(other.glyphAtlasGeneration, 0)) {}

VulkanFrameRecorder &VulkanFrameRecorder::operator=(VulkanFrameRecorder &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  clear();
  ops = std::move(other.ops);
  quads = std::move(other.quads);
  rects = std::move(other.rects);
  pathVerts = std::move(other.pathVerts);
  preparedQuadBuffer = std::exchange(other.preparedQuadBuffer, VK_NULL_HANDLE);
  preparedQuadAllocation = std::exchange(other.preparedQuadAllocation, VK_NULL_HANDLE);
  preparedQuadCapacity = std::exchange(other.preparedQuadCapacity, 0);
  preparedRectBuffer = std::exchange(other.preparedRectBuffer, VK_NULL_HANDLE);
  preparedRectAllocation = std::exchange(other.preparedRectAllocation, VK_NULL_HANDLE);
  preparedRectCapacity = std::exchange(other.preparedRectCapacity, 0);
  preparedPathVertexBuffer = std::exchange(other.preparedPathVertexBuffer, VK_NULL_HANDLE);
  preparedPathVertexAllocation = std::exchange(other.preparedPathVertexAllocation, VK_NULL_HANDLE);
  preparedPathVertexCapacity = std::exchange(other.preparedPathVertexCapacity, 0);
  allocator = std::exchange(other.allocator, VK_NULL_HANDLE);
  rootClip = other.rootClip;
  glyphAtlasGeneration = std::exchange(other.glyphAtlasGeneration, 0);
  return *this;
}

void VulkanFrameRecorder::clear() {
  if (allocator) {
    destroyRecorderBuffer(allocator, preparedQuadBuffer, preparedQuadAllocation, preparedQuadCapacity);
    destroyRecorderBuffer(allocator, preparedRectBuffer, preparedRectAllocation, preparedRectCapacity);
    destroyRecorderBuffer(allocator, preparedPathVertexBuffer, preparedPathVertexAllocation,
                          preparedPathVertexCapacity);
  } else {
    preparedQuadBuffer = VK_NULL_HANDLE;
    preparedQuadAllocation = VK_NULL_HANDLE;
    preparedQuadCapacity = 0;
    preparedRectBuffer = VK_NULL_HANDLE;
    preparedRectAllocation = VK_NULL_HANDLE;
    preparedRectCapacity = 0;
    preparedPathVertexBuffer = VK_NULL_HANDLE;
    preparedPathVertexAllocation = VK_NULL_HANDLE;
    preparedPathVertexCapacity = 0;
  }
  allocator = VK_NULL_HANDLE;
  ops.clear();
  quads.clear();
  rects.clear();
  pathVerts.clear();
  rootClip = {};
  glyphAtlasGeneration = 0;
}

} // namespace flux
