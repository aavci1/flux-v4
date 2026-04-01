#include <cstdlib>
#include <cstring>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace flux::detail {

std::vector<std::uint8_t> encodePngFromRgba(std::vector<std::uint8_t> const& rgba, int w, int h) {
  std::vector<std::uint8_t> out;
  if (w <= 0 || h <= 0 || rgba.size() < static_cast<std::size_t>(w * h * 4)) {
    return out;
  }
  stbi_write_png_to_func(
      [](void* ctx, void* data, int size) {
        auto* buf = static_cast<std::vector<std::uint8_t>*>(ctx);
        auto* bytes = static_cast<std::uint8_t*>(data);
        buf->insert(buf->end(), bytes, bytes + static_cast<std::size_t>(size));
      },
      &out, w, h, 4, rgba.data(), w * 4);
  return out;
}

} // namespace flux::detail
