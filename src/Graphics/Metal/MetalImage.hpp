#pragma once

#include <Flux/Graphics/Image.hpp>

#import <Metal/Metal.h>

namespace flux {

/// Metal-backed `Image` (BGRA/RGBA texture from disk loader).
class MetalImage final : public Image {
public:
  explicit MetalImage(id<MTLTexture> texture);

  Size size() const override;

  id<MTLTexture> texture() const { return texture_; }

private:
  id<MTLTexture> texture_{nil};
};

/// Returns null if `image` is not a `MetalImage`.
MetalImage const* tryMetalImage(Image const& image);

} // namespace flux
