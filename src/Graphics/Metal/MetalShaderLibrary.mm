#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <dispatch/dispatch.h>

#include "Graphics/Metal/MetalShaderLibrary.hpp"

#include "FluxShaders.metallib.h"

#include <stdexcept>

namespace flux::detail {

id<MTLLibrary> fluxLoadShaderLibrary(id<MTLDevice> device) {
  static NSMutableDictionary* cache = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    cache = [NSMutableDictionary dictionary];
  });

  NSNumber* key = @((uintptr_t)(__bridge void*)device);
  @synchronized(cache) {
    id<MTLLibrary> existing = cache[key];
    if (existing) {
      return existing;
    }

    NSError* err = nil;
    dispatch_data_t libData = dispatch_data_create(
        FluxShaders_metallib, static_cast<size_t>(FluxShaders_metallib_len), nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
    id<MTLLibrary> lib = [device newLibraryWithData:libData error:&err];
    if (!lib) {
      NSLog(@"Flux: failed to load embedded metallib: %@", err);
      throw std::runtime_error("flux: embedded metallib load failed");
    }
    cache[key] = lib;
    return lib;
  }
}

} // namespace flux::detail
