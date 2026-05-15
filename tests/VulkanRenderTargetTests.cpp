#include <doctest/doctest.h>

#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/RenderTarget.hpp>
#include <Flux/Graphics/VulkanContext.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>

#if FLUX_VULKAN

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using namespace flux;
using namespace flux::scenegraph;

void vkCheck(VkResult result, char const* label) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(label);
  }
}

std::uint32_t findMemoryType(VkPhysicalDevice physical, std::uint32_t typeBits,
                             VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memoryProperties{};
  vkGetPhysicalDeviceMemoryProperties(physical, &memoryProperties);
  for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
    bool const typeMatches = (typeBits & (1u << i)) != 0;
    bool const propertiesMatch =
        (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
    if (typeMatches && propertiesMatch) {
      return i;
    }
  }
  throw std::runtime_error("No compatible Vulkan memory type");
}

struct VulkanImageTarget {
  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
  std::uint32_t width = 0;
  std::uint32_t height = 0;

  VulkanImageTarget(VkPhysicalDevice physicalDevice, VkDevice logicalDevice,
                    std::uint32_t targetWidth, std::uint32_t targetHeight)
      : device(logicalDevice), physical(physicalDevice), width(targetWidth), height(targetHeight) {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCheck(vkCreateImage(device, &imageInfo, nullptr, &image), "vkCreateImage");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, image, &requirements);

    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex =
        findMemoryType(physical, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkCheck(vkAllocateMemory(device, &allocateInfo, nullptr, &memory), "vkAllocateMemory");
    vkCheck(vkBindImageMemory(device, image, memory, 0), "vkBindImageMemory");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    vkCheck(vkCreateImageView(device, &viewInfo, nullptr, &view), "vkCreateImageView");
  }

  ~VulkanImageTarget() {
    if (view) {
      vkDestroyImageView(device, view, nullptr);
    }
    if (image) {
      vkDestroyImage(device, image, nullptr);
    }
    if (memory) {
      vkFreeMemory(device, memory, nullptr);
    }
  }
};

struct VulkanReadbackBuffer {
  VkDevice device = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize size = 0;

  VulkanReadbackBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkDeviceSize byteSize)
      : device(logicalDevice), physical(physicalDevice), size(byteSize) {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(
        physical, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkCheck(vkAllocateMemory(device, &allocateInfo, nullptr, &memory), "vkAllocateMemory");
    vkCheck(vkBindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");
  }

  ~VulkanReadbackBuffer() {
    if (buffer) {
      vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory) {
      vkFreeMemory(device, memory, nullptr);
    }
  }
};

struct VulkanCopyContext {
  VkDevice device = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  VkCommandPool pool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkFence fence = VK_NULL_HANDLE;

  VulkanCopyContext(VkDevice logicalDevice, VkQueue renderQueue, std::uint32_t queueFamily)
      : device(logicalDevice), queue(renderQueue) {
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    vkCheck(vkCreateCommandPool(device, &poolInfo, nullptr, &pool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = pool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    vkCheck(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer),
            "vkAllocateCommandBuffers");

    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vkCheck(vkCreateFence(device, &fenceInfo, nullptr, &fence), "vkCreateFence");
  }

  ~VulkanCopyContext() {
    if (fence) {
      vkDestroyFence(device, fence, nullptr);
    }
    if (pool) {
      vkDestroyCommandPool(device, pool, nullptr);
    }
  }

  void copyImageToBuffer(VkImage image, VkBuffer buffer, std::uint32_t width, std::uint32_t height) {
    vkCheck(vkResetCommandBuffer(commandBuffer, 0), "vkResetCommandBuffer");
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {width, height, 1};
    vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer, 1, &copy);
    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;
    vkCheck(vkResetFences(device, 1, &fence), "vkResetFences");
    vkCheck(vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit");
    vkCheck(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
  }
};

} // namespace

TEST_CASE("Vulkan RenderTarget renders canvas ops into an offscreen image") {
  auto& vk = VulkanContext::instance();
  vk.ensureInitialized();

  VkPhysicalDevice physical = vk.physicalDevice();
  VkDevice device = vk.device();
  REQUIRE(physical != VK_NULL_HANDLE);
  REQUIRE(device != VK_NULL_HANDLE);

  constexpr std::uint32_t width = 64;
  constexpr std::uint32_t height = 64;
  VulkanImageTarget targetImage{physical, device, width, height};
  std::shared_ptr<Image> externalImage =
      Image::fromExternalVulkan(targetImage.image, targetImage.view, targetImage.format,
                                width, height);
  REQUIRE(externalImage);
  CHECK(externalImage->size().width == static_cast<float>(width));
  CHECK(externalImage->size().height == static_cast<float>(height));

  flux::RenderTarget target{flux::VulkanRenderTargetSpec{
      .image = targetImage.image,
      .view = targetImage.view,
      .format = targetImage.format,
      .width = width,
      .height = height,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
  }};

  target.beginFrame();
  target.canvas().clear(flux::Colors::black);
  auto root = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 64.f, 64.f});
  root->appendChild(std::make_unique<RectNode>(
      flux::Rect{16.f, 16.f, 32.f, 32.f},
      flux::FillStyle::solid(flux::Color{1.f, 0.f, 0.f, 1.f})));
  SceneGraph graph{std::move(root)};
  target.renderScene(graph);
  target.endFrame();

  VulkanReadbackBuffer readback{physical, device, width * height * 4u};
  VulkanCopyContext copy{device, vk.queue(), vk.queueFamily()};
  copy.copyImageToBuffer(targetImage.image, readback.buffer, width, height);

  std::vector<std::uint8_t> pixels(width * height * 4u);
  void* mapped = nullptr;
  vkCheck(vkMapMemory(device, readback.memory, 0, readback.size, 0, &mapped), "vkMapMemory");
  std::memcpy(pixels.data(), mapped, pixels.size());
  vkUnmapMemory(device, readback.memory);

  std::size_t const center = (32u * static_cast<std::size_t>(width) + 32u) * 4u;
  CHECK(pixels[center + 2] > 200);
  CHECK(pixels[center + 1] < 32);
  CHECK(pixels[center + 0] < 32);
}

#endif // FLUX_VULKAN
