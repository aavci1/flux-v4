#pragma once

/// \file Flux/Graphics/Image.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Geometry.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#if FLUX_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace flux {

/// Abstract image reference; pixel dimensions drive UV normalization in `Canvas::drawImage`.
class Image {
public:
  virtual ~Image() = default;

  Image(Image const&) = delete;
  Image& operator=(Image const&) = delete;

  virtual Size size() const = 0;

  /// Create an image from tightly packed 8-bit RGBA pixels.
  /// `rgbaPixels` must contain exactly width * height * 4 bytes.
  /// Metal uses `gpuDevice` as an optional id<MTLDevice>; other backends ignore it.
  static std::shared_ptr<Image> fromRgbaPixels(std::uint32_t width, std::uint32_t height,
                                               std::span<std::uint8_t const> rgbaPixels,
                                               void* gpuDevice = nullptr);

#if FLUX_VULKAN
  /// Create an image reference backed by caller-owned Vulkan resources.
  /// The VkImage and VkImageView must outlive all rendering that references the returned Image.
  static std::shared_ptr<Image> fromExternalVulkan(VkImage image, VkImageView view, VkFormat format,
                                                   std::uint32_t width, std::uint32_t height);
#endif

#if FLUX_METAL
  /// Create an image reference backed by a caller-owned id<MTLTexture>.
  /// The texture must outlive all rendering that references the returned Image.
  static std::shared_ptr<Image> fromExternalMetal(void* texture, std::uint32_t width, std::uint32_t height);
#endif

protected:
  Image() = default;
};

/// Loads an image from disk into a GPU texture (Metal). `gpuDevice` must be the same device as the drawable
/// (`Canvas::gpuDevice()`); pass null only on single-GPU systems (uses the system default device).
std::shared_ptr<Image> loadImageFromFile(std::string_view path, void* gpuDevice = nullptr);

} // namespace flux
