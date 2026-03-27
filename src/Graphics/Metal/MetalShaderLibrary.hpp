#pragma once

#import <Metal/Metal.h>

namespace flux::detail {

/// Loads the embedded `FluxShaders.metallib` once per `MTLDevice` (cached).
id<MTLLibrary> fluxLoadShaderLibrary(id<MTLDevice> device);

} // namespace flux::detail
