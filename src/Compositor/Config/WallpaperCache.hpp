#pragma once

#include <Flux/Graphics/Image.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace flux::compositor {

[[nodiscard]] std::filesystem::path wallpaperCacheDirectory(std::filesystem::path const& cacheRoot);

[[nodiscard]] std::optional<flux::DecodedImageRgba>
readWallpaperCache(std::filesystem::path const& sourcePath,
                   std::uint32_t maxLongEdge,
                   std::filesystem::path const& cacheRoot);

[[nodiscard]] bool writeWallpaperCache(std::filesystem::path const& sourcePath,
                                       std::uint32_t maxLongEdge,
                                       std::filesystem::path const& cacheRoot,
                                       flux::DecodedImageRgba const& image);

} // namespace flux::compositor
