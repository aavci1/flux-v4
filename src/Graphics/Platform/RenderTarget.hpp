#pragma once

#include <Flux/Graphics/RenderTarget.hpp>

#include <memory>

namespace flux::platform {

class RenderTarget {
public:
  virtual ~RenderTarget() = default;

  virtual Canvas& canvas() = 0;
  virtual void beginFrame() = 0;
  virtual void endFrame() = 0;
};

#if FLUX_VULKAN
std::unique_ptr<RenderTarget> createRenderTarget(VulkanRenderTargetSpec const& spec);
#endif

#if FLUX_METAL
std::unique_ptr<RenderTarget> createRenderTarget(MetalRenderTargetSpec const& spec);
#endif

} // namespace flux::platform
