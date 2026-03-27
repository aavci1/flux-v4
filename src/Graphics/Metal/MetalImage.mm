#import <Foundation/Foundation.h>
#import <MetalKit/MetalKit.h>

#include "Graphics/Metal/MetalImage.hpp"

#include <string>

namespace flux {

MetalImage::MetalImage(id<MTLTexture> texture) : texture_(texture) {}

Size MetalImage::size() const {
  if (!texture_) {
    return {};
  }
  return Size{static_cast<float>(texture_.width), static_cast<float>(texture_.height)};
}

MetalImage const* tryMetalImage(Image const& image) {
  return dynamic_cast<MetalImage const*>(&image);
}

std::shared_ptr<Image> loadImageFromFile(std::string_view path) {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (!device) {
    return nullptr;
  }
  NSString* nsPath = [[NSString alloc] initWithBytes:path.data() length:path.size()
                                              encoding:NSUTF8StringEncoding];
  if (!nsPath) {
    return nullptr;
  }
  NSError* err = nil;
  MTKTextureLoader* loader = [[MTKTextureLoader alloc] initWithDevice:device];
  NSDictionary* opts = @{
    MTKTextureLoaderOptionTextureUsage : @(MTLTextureUsageShaderRead),
    MTKTextureLoaderOptionSRGB : @(NO),
  };
  id<MTLTexture> tex = [loader newTextureWithContentsOfURL:[NSURL fileURLWithPath:nsPath] options:opts error:&err];
  if (!tex) {
    return nullptr;
  }
  return std::make_shared<MetalImage>(tex);
}

} // namespace flux
