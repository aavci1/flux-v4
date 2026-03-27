#pragma once

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

/// Loads an image from disk into a GPU-backed texture (Metal). Returns null on failure.
std::shared_ptr<Image> loadImageFromFile(std::string_view path);

} // namespace flux
