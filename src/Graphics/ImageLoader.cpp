#include <Flux/Graphics/Image.hpp>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_STDIO
#include "stb_image.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace flux {

std::shared_ptr<Image> loadImage(std::string_view path, void* gpuDevice) {
  std::ifstream in(std::filesystem::path(std::string(path)), std::ios::binary);
  if (!in) {
    return nullptr;
  }

  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (data.empty() || data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return nullptr;
  }

  int width = 0;
  int height = 0;
  auto const* bytes = reinterpret_cast<stbi_uc const*>(data.data());
  stbi_uc* decoded = stbi_load_from_memory(bytes, static_cast<int>(data.size()), &width, &height, nullptr,
                                           STBI_rgb_alpha);
  if (!decoded || width <= 0 || height <= 0) {
    if (decoded) {
      stbi_image_free(decoded);
    }
    return nullptr;
  }

  std::size_t const pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  if (pixelCount > std::numeric_limits<std::size_t>::max() / 4u) {
    stbi_image_free(decoded);
    return nullptr;
  }

  std::span<std::uint8_t const> pixels(decoded, pixelCount * 4u);
  std::shared_ptr<Image> image =
      Image::fromRgbaPixels(static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), pixels,
                            gpuDevice);
  stbi_image_free(decoded);
  return image;
}

} // namespace flux
