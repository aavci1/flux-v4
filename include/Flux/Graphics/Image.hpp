#pragma once

/// \file Flux/Graphics/Image.hpp
///
/// Part of the Flux public API.


#include <Flux/Core/Types.hpp>

#include <memory>
#include <string_view>

namespace flux {

/// Abstract image reference; pixel dimensions drive UV normalization in `Canvas::drawImage`.
class Image {
public:
  virtual ~Image() = default;

  Image(Image const&) = delete;
  Image& operator=(Image const&) = delete;

  virtual Size size() const = 0;

protected:
  Image() = default;
};

/// Loads an image from disk into a GPU texture (Metal). `gpuDevice` must be the same device as the drawable
/// (`Canvas::gpuDevice()`); pass null only on single-GPU systems (uses the system default device).
std::shared_ptr<Image> loadImageFromFile(std::string_view path, void* gpuDevice = nullptr);

} // namespace flux
