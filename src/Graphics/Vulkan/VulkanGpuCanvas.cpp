#include "Graphics/Vulkan/VulkanCanvas.hpp"

#include <Flux/Debug/PerfCounters.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/TextSystem.hpp>

#include "Graphics/Vulkan/generated/image_frag_spv.hpp"
#include "Graphics/Vulkan/generated/image_vert_spv.hpp"
#include "Graphics/Vulkan/generated/path_frag_spv.hpp"
#include "Graphics/Vulkan/generated/path_vert_spv.hpp"
#include "Graphics/Vulkan/generated/rect_frag_spv.hpp"
#include "Graphics/Vulkan/generated/rect_vert_spv.hpp"
#include "Graphics/PathFlattener.hpp"

#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>
#include <wayland-client.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace flux {
namespace {

struct Rgba {
  std::uint8_t r = 0, g = 0, b = 0, a = 255;
};

struct VulkanImage final : Image {
  int width = 0;
  int height = 0;
  std::vector<Rgba> pixels;
  mutable VkImage image = VK_NULL_HANDLE;
  mutable VkDeviceMemory memory = VK_NULL_HANDLE;
  mutable VkImageView view = VK_NULL_HANDLE;
  mutable VkDescriptorSet descriptor = VK_NULL_HANDLE;
  mutable bool uploaded = false;

  VulkanImage(int w, int h, std::vector<Rgba> p) : width(w), height(h), pixels(std::move(p)) {}
  ~VulkanImage() override = default;
  Size size() const override { return {static_cast<float>(width), static_cast<float>(height)}; }
};

struct Buffer {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkDeviceSize capacity = 0;
};

struct Texture {
  VkImage image = VK_NULL_HANDLE;
  VkDeviceMemory memory = VK_NULL_HANDLE;
  VkImageView view = VK_NULL_HANDLE;
  VkDescriptorSet descriptor = VK_NULL_HANDLE;
  int width = 0;
  int height = 0;
};

struct RectInstance {
  float rect[4]{};
  float axisX[4]{};
  float axisY[4]{};
  float radii[4]{};
  float fill0[4]{};
  float fill1[4]{};
  float fill2[4]{};
  float fill3[4]{};
  float stops[4]{};
  float gradient[4]{};
  float stroke[4]{};
  float params[4]{};
};

struct QuadInstance {
  float rect[4]{};
  float uv[4]{};
  float color[4]{};
};

struct ImageBatch {
  Texture* texture = nullptr;
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

struct DrawOp {
  enum class Kind : std::uint8_t { Rect, Path, Image };
  Kind kind = Kind::Rect;
  Texture* texture = nullptr;
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

struct GlyphKey {
  std::uint32_t fontId = 0;
  std::uint16_t glyphId = 0;
  std::uint16_t size = 0;
  bool operator==(GlyphKey const&) const = default;
};

struct GlyphKeyHash {
  std::size_t operator()(GlyphKey const& k) const noexcept {
    return (static_cast<std::size_t>(k.fontId) << 32u) ^
           (static_cast<std::size_t>(k.glyphId) << 16u) ^ k.size;
  }
};

struct GlyphSlot {
  float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
  std::uint32_t w = 0, h = 0;
  Point bearing{};
};

void putColor(float out[4], Color c, float opacity = 1.f) {
  out[0] = c.r;
  out[1] = c.g;
  out[2] = c.b;
  out[3] = c.a * opacity;
}

Rect unionRects(Rect a, Rect b) {
  float const x0 = std::min(a.x, b.x);
  float const y0 = std::min(a.y, b.y);
  float const x1 = std::max(a.x + a.width, b.x + b.width);
  float const y1 = std::max(a.y + a.height, b.y + b.height);
  return Rect::sharp(x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0));
}

void vkCheck(VkResult result, char const* what) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::string(what) + " failed");
  }
}

std::uint32_t findMemoryType(VkPhysicalDevice physical, std::uint32_t typeBits, VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties props{};
  vkGetPhysicalDeviceMemoryProperties(physical, &props);
  for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i) {
    if ((typeBits & (1u << i)) && ((props.memoryTypes[i].propertyFlags & flags) == flags)) {
      return i;
    }
  }
  throw std::runtime_error("No compatible Vulkan memory type");
}

CornerRadius clampRadii(CornerRadius r, float w, float h) {
  float const maxR = std::max(0.f, std::min(w, h) * 0.5f);
  r.topLeft = std::clamp(r.topLeft, 0.f, maxR);
  r.topRight = std::clamp(r.topRight, 0.f, maxR);
  r.bottomRight = std::clamp(r.bottomRight, 0.f, maxR);
  r.bottomLeft = std::clamp(r.bottomLeft, 0.f, maxR);
  auto fit = [](float& a, float& b, float len) {
    if (a + b > len && len > 0.f) {
      float s = len / (a + b);
      a *= s;
      b *= s;
    }
  };
  fit(r.topLeft, r.topRight, w);
  fit(r.bottomLeft, r.bottomRight, w);
  fit(r.topLeft, r.bottomLeft, h);
  fit(r.topRight, r.bottomRight, h);
  return r;
}

VkShaderModule shaderModule(VkDevice device, unsigned char const* bytes, unsigned int len) {
  VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  info.codeSize = len;
  info.pCode = reinterpret_cast<std::uint32_t const*>(bytes);
  VkShaderModule module = VK_NULL_HANDLE;
  vkCheck(vkCreateShaderModule(device, &info, nullptr, &module), "vkCreateShaderModule");
  return module;
}

bool representativeFillColor(FillStyle const& fill, Color* out) {
  if (fill.solidColor(out)) return true;
  LinearGradient linear{};
  if (fill.linearGradient(&linear) && linear.stopCount > 0) {
    *out = linear.stops[0].color;
    return true;
  }
  RadialGradient radial{};
  if (fill.radialGradient(&radial) && radial.stopCount > 0) {
    *out = radial.stops[0].color;
    return true;
  }
  ConicalGradient conical{};
  if (fill.conicalGradient(&conical) && conical.stopCount > 0) {
    *out = conical.stops[0].color;
    return true;
  }
  return false;
}

Rect boundsOfSubpaths(std::vector<std::vector<Point>> const& subpaths) {
  float minX = std::numeric_limits<float>::infinity();
  float minY = std::numeric_limits<float>::infinity();
  float maxX = -std::numeric_limits<float>::infinity();
  float maxY = -std::numeric_limits<float>::infinity();
  for (auto const& subpath : subpaths) {
    for (Point const& point : subpath) {
      minX = std::min(minX, point.x);
      minY = std::min(minY, point.y);
      maxX = std::max(maxX, point.x);
      maxY = std::max(maxY, point.y);
    }
  }
  if (!std::isfinite(minX) || maxX <= minX || maxY <= minY) {
    return Rect::sharp(0.f, 0.f, 1.f, 1.f);
  }
  return Rect::sharp(minX, minY, maxX - minX, maxY - minY);
}

Color interpolateStops(GradientStop const* stops, std::uint8_t stopCount, float t, float opacity) {
  t = std::clamp(t, 0.f, 1.f);
  if (stopCount == 0) return Colors::transparent;
  if (stopCount == 1 || t <= stops[0].position) {
    Color c = stops[0].color;
    c.a *= opacity;
    return c;
  }
  for (std::uint8_t i = 0; i + 1 < stopCount; ++i) {
    GradientStop const& a = stops[i];
    GradientStop const& b = stops[i + 1];
    if (t <= b.position || i + 2 == stopCount) {
      float const span = std::max(b.position - a.position, 1e-5f);
      float const u = std::clamp((t - a.position) / span, 0.f, 1.f);
      return {a.color.r + (b.color.r - a.color.r) * u,
              a.color.g + (b.color.g - a.color.g) * u,
              a.color.b + (b.color.b - a.color.b) * u,
              (a.color.a + (b.color.a - a.color.a) * u) * opacity};
    }
  }
  Color c = stops[stopCount - 1].color;
  c.a *= opacity;
  return c;
}

bool gradientColorAt(FillStyle const& fill, Point unit, float opacity, Color* out) {
  LinearGradient linear{};
  if (fill.linearGradient(&linear) && linear.stopCount >= 2) {
    Point const axis = linear.end - linear.start;
    float const axisLenSq = axis.x * axis.x + axis.y * axis.y;
    float const t = axisLenSq > 1e-8f
                        ? ((unit.x - linear.start.x) * axis.x + (unit.y - linear.start.y) * axis.y) / axisLenSq
                        : 0.f;
    *out = interpolateStops(linear.stops.data(), linear.stopCount, t, opacity);
    return true;
  }
  RadialGradient radial{};
  if (fill.radialGradient(&radial) && radial.stopCount >= 2) {
    float const dx = unit.x - radial.center.x;
    float const dy = unit.y - radial.center.y;
    *out = interpolateStops(radial.stops.data(), radial.stopCount,
                            std::sqrt(dx * dx + dy * dy) / std::max(radial.radius, 1e-4f), opacity);
    return true;
  }
  ConicalGradient conical{};
  if (fill.conicalGradient(&conical) && conical.stopCount >= 2) {
    constexpr float twoPi = 6.28318530718f;
    float t = (std::atan2(unit.y - conical.center.y, unit.x - conical.center.x) -
               conical.startAngleRadians) / twoPi;
    t -= std::floor(t);
    *out = interpolateStops(conical.stops.data(), conical.stopCount, t, opacity);
    return true;
  }
  return false;
}

void applyGradientFill(TessellatedPath& tessellated, FillStyle const& fill, Rect bounds, float opacity) {
  float const invW = 1.f / std::max(bounds.width, 1e-4f);
  float const invH = 1.f / std::max(bounds.height, 1e-4f);
  for (PathVertex& vertex : tessellated.vertices) {
    Point const unit{(vertex.x - bounds.x) * invW, (vertex.y - bounds.y) * invH};
    Color color{};
    if (!gradientColorAt(fill, unit, opacity, &color)) return;
    vertex.color[0] = color.r;
    vertex.color[1] = color.g;
    vertex.color[2] = color.b;
    vertex.color[3] = color.a;
  }
}

} // namespace

class VulkanCanvas final : public Canvas {
public:
  VulkanCanvas(wl_display* display, wl_surface* surface, unsigned int handle, TextSystem& textSystem)
      : display_(display), wlSurface_(surface), handle_(handle), textSystem_(textSystem) {
    createInstance();
    VkWaylandSurfaceCreateInfoKHR surfaceInfo{VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
    surfaceInfo.display = display_;
    surfaceInfo.surface = wlSurface_;
    vkCheck(vkCreateWaylandSurfaceKHR(instance_, &surfaceInfo, nullptr, &surface_), "vkCreateWaylandSurfaceKHR");
    pickDevice();
    createDevice();
    createCommandObjects();
    createDescriptors();
    createSampler();
    chooseSurfaceFormat();
    createRenderPass();
    createPipelines();
    createAtlas();
  }

  ~VulkanCanvas() override {
    if (device_) {
      vkDeviceWaitIdle(device_);
    }
    destroySwapchain();
    destroyTexture(atlas_);
    for (auto& kv : imageTextures_) {
      destroyTexture(kv.second);
    }
    destroyBuffer(pathBuffer_);
    destroyBuffer(rectBuffer_);
    destroyBuffer(quadBuffer_);
    if (sampler_) vkDestroySampler(device_, sampler_, nullptr);
    if (pathPipeline_) vkDestroyPipeline(device_, pathPipeline_, nullptr);
    if (rectPipeline_) vkDestroyPipeline(device_, rectPipeline_, nullptr);
    if (imagePipeline_) vkDestroyPipeline(device_, imagePipeline_, nullptr);
    if (pathPipelineLayout_) vkDestroyPipelineLayout(device_, pathPipelineLayout_, nullptr);
    if (rectPipelineLayout_) vkDestroyPipelineLayout(device_, rectPipelineLayout_, nullptr);
    if (imagePipelineLayout_) vkDestroyPipelineLayout(device_, imagePipelineLayout_, nullptr);
    if (rectDescriptorLayout_) vkDestroyDescriptorSetLayout(device_, rectDescriptorLayout_, nullptr);
    if (quadDescriptorLayout_) vkDestroyDescriptorSetLayout(device_, quadDescriptorLayout_, nullptr);
    if (textureDescriptorLayout_) vkDestroyDescriptorSetLayout(device_, textureDescriptorLayout_, nullptr);
    if (descriptorPool_) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);
    if (imageAvailable_) vkDestroySemaphore(device_, imageAvailable_, nullptr);
    if (renderFinished_) vkDestroySemaphore(device_, renderFinished_, nullptr);
    if (frameFence_) vkDestroyFence(device_, frameFence_, nullptr);
    if (commandPool_) vkDestroyCommandPool(device_, commandPool_, nullptr);
    if (device_) vkDestroyDevice(device_, nullptr);
    if (surface_) vkDestroySurfaceKHR(instance_, surface_, nullptr);
    if (instance_) vkDestroyInstance(instance_, nullptr);
  }

  Backend backend() const noexcept override { return Backend::Vulkan; }
  unsigned int windowHandle() const override { return handle_; }

  void resize(int width, int height) override {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    int const fbW = std::max(1, static_cast<int>(std::lround(static_cast<float>(width_) * dpiScaleX_)));
    int const fbH = std::max(1, static_cast<int>(std::lround(static_cast<float>(height_) * dpiScaleY_)));
    if (fbW != framebufferWidth_ || fbH != framebufferHeight_) {
      framebufferWidth_ = fbW;
      framebufferHeight_ = fbH;
      swapchainDirty_ = true;
    }
  }

  void updateDpiScale(float sx, float sy) override {
    dpiScaleX_ = std::max(0.25f, sx);
    dpiScaleY_ = std::max(0.25f, sy);
    resize(width_, height_);
  }

  void beginFrame() override {
    if (swapchainDirty_ || !swapchain_) {
      recreateSwapchain();
    }
    rects_.clear();
    quads_.clear();
    batches_.clear();
    ops_.clear();
    pathVerts_.clear();
    stateStack_.clear();
    state_ = {};
    state_.clip = Rect::sharp(0.f, 0.f, static_cast<float>(width_), static_cast<float>(height_));
  }

  void clear(Color color = Colors::transparent) override { clearColor_ = color; }

  void present() override {
    if (!swapchain_ || width_ <= 0 || height_ <= 0 || framebufferWidth_ <= 0 || framebufferHeight_ <= 0) return;
    debug::perf::ScopedTimer timer(debug::perf::TimedMetric::CanvasPresent);
    vkWaitForFences(device_, 1, &frameFence_, VK_TRUE, UINT64_MAX);
    std::uint32_t imageIndex = 0;
    VkResult acquired = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailable_, VK_NULL_HANDLE,
                                              &imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapchain();
      return;
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
      vkCheck(acquired, "vkAcquireNextImageKHR");
    }
    vkResetFences(device_, 1, &frameFence_);

    uploadFrameBuffers();
    uploadAtlasIfNeeded();

    vkResetCommandBuffer(commandBuffer_, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkCheck(vkBeginCommandBuffer(commandBuffer_, &begin), "vkBeginCommandBuffer");
    VkClearValue clear{};
    clear.color.float32[0] = clearColor_.r;
    clear.color.float32[1] = clearColor_.g;
    clear.color.float32[2] = clearColor_.b;
    clear.color.float32[3] = clearColor_.a;
    VkRenderPassBeginInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rp.renderPass = renderPass_;
    rp.framebuffer = framebuffers_[imageIndex];
    rp.renderArea.extent = swapExtent_;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(commandBuffer_, &rp, VK_SUBPASS_CONTENTS_INLINE);
    drawOps();
    vkCmdEndRenderPass(commandBuffer_);
    transition(commandBuffer_, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    writeDebugScreenshotIfRequested(swapchainImages_[imageIndex]);
    transition(commandBuffer_, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkCheck(vkEndCommandBuffer(commandBuffer_), "vkEndCommandBuffer");

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imageAvailable_;
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer_;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinished_;
    vkCheck(vkQueueSubmit(queue_, 1, &submit, frameFence_), "vkQueueSubmit");
    flushScreenshot();

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished_;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;
    VkResult presented = vkQueuePresentKHR(queue_, &presentInfo);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR) {
      recreateSwapchain();
    } else {
      vkCheck(presented, "vkQueuePresentKHR");
    }
    debug::perf::recordPresentedFrame();
  }

  void save() override { stateStack_.push_back(state_); }
  void restore() override {
    if (!stateStack_.empty()) {
      state_ = stateStack_.back();
      stateStack_.pop_back();
    }
  }
  void setTransform(Mat3 const& m) override { state_.transform = m; }
  void transform(Mat3 const& m) override { state_.transform = state_.transform * m; }
  void translate(Point p) override { transform(Mat3::translate(p)); }
  void translate(float x, float y) override { translate({x, y}); }
  void scale(float sx, float sy) override { transform(Mat3::scale(sx, sy)); }
  void scale(float s) override { scale(s, s); }
  void rotate(float r) override { transform(Mat3::rotate(r)); }
  void rotate(float r, Point p) override { transform(Mat3::rotate(r, p)); }
  Mat3 currentTransform() const override { return state_.transform; }

  void clipRect(Rect rect, CornerRadius const&, bool) override {
    Rect r = transformedBounds(rect);
    float x0 = std::max(state_.clip.x, r.x);
    float y0 = std::max(state_.clip.y, r.y);
    float x1 = std::min(state_.clip.x + state_.clip.width, r.x + r.width);
    float y1 = std::min(state_.clip.y + state_.clip.height, r.y + r.height);
    state_.clip = Rect::sharp(x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0));
  }
  Rect clipBounds() const override { return state_.clip; }
  bool quickReject(Rect rect) const override { return !state_.clip.intersects(transformedBounds(rect)); }
  void setOpacity(float opacity) override { state_.opacity = std::clamp(opacity, 0.f, 1.f); }
  float opacity() const override { return state_.opacity; }
  void setBlendMode(BlendMode mode) override { state_.blendMode = mode; }
  BlendMode blendMode() const override { return state_.blendMode; }

  void pushRectInstance(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                        StrokeStyle const& stroke, float opacity) {
    if (rect.width <= 0.f || rect.height <= 0.f) return;
    Point const p0 = state_.transform.apply({rect.x, rect.y});
    Point const p1 = state_.transform.apply({rect.x + rect.width, rect.y});
    Point const p3 = state_.transform.apply({rect.x, rect.y + rect.height});
    RectInstance inst{};
    inst.rect[0] = 0.f; inst.rect[1] = 0.f; inst.rect[2] = rect.width; inst.rect[3] = rect.height;
    inst.axisX[0] = p0.x; inst.axisX[1] = p0.y; inst.axisX[2] = p1.x - p0.x; inst.axisX[3] = p1.y - p0.y;
    inst.axisY[0] = p3.x - p0.x; inst.axisY[1] = p3.y - p0.y;
    CornerRadius cr = clampRadii(cornerRadius, rect.width, rect.height);
    inst.radii[0] = cr.topLeft; inst.radii[1] = cr.topRight;
    inst.radii[2] = cr.bottomRight; inst.radii[3] = cr.bottomLeft;
    encodeFill(fill, inst);
    Color sc{};
    if (stroke.solidColor(&sc) && stroke.width > 0.f) {
      putColor(inst.stroke, sc, opacity);
      inst.params[2] = stroke.width;
    }
    inst.params[3] = opacity;
    std::uint32_t first = static_cast<std::uint32_t>(rects_.size());
    rects_.push_back(inst);
    ops_.push_back(DrawOp{DrawOp::Kind::Rect, nullptr, first, 1});
    debug::perf::recordDrawCall(debug::perf::RenderCounterKind::Rect);
  }

  void drawRect(Rect const& rect, CornerRadius const& cornerRadius, FillStyle const& fill,
                StrokeStyle const& stroke, ShadowStyle const& shadow) override {
    if (rect.width <= 0.f || rect.height <= 0.f) return;
    Rect rejectBounds = transformedBounds(rect);
    if (!shadow.isNone()) {
      float const pad = shadow.radius;
      Rect const shadowRect = Rect::sharp(rect.x + shadow.offset.x - pad, rect.y + shadow.offset.y - pad,
                                          rect.width + pad * 2.f, rect.height + pad * 2.f);
      rejectBounds = unionRects(rejectBounds, transformedBounds(shadowRect));
    }
    if (rejectBounds.width <= 0.f || rejectBounds.height <= 0.f || !state_.clip.intersects(rejectBounds)) return;

    if (!shadow.isNone()) {
      if (shadow.radius <= 0.f) {
        Rect const layer = Rect::sharp(rect.x + shadow.offset.x, rect.y + shadow.offset.y,
                                       rect.width, rect.height);
        pushRectInstance(layer, cornerRadius, FillStyle::solid(shadow.color), StrokeStyle::none(), state_.opacity);
      } else {
        int const steps = std::clamp(static_cast<int>(std::ceil(shadow.radius / 3.f)), 3, 8);
        for (int i = steps; i >= 1; --i) {
          float const t = static_cast<float>(i) / static_cast<float>(steps);
          float const spread = shadow.radius * t;
          float const alpha = shadow.color.a * state_.opacity * (1.f - t * 0.72f) / static_cast<float>(steps);
          Color c = shadow.color;
          c.a = alpha;
          Rect const layer = Rect::sharp(rect.x + shadow.offset.x - spread,
                                         rect.y + shadow.offset.y - spread,
                                         rect.width + spread * 2.f,
                                         rect.height + spread * 2.f);
          CornerRadius cr{cornerRadius.topLeft + spread, cornerRadius.topRight + spread,
                          cornerRadius.bottomRight + spread, cornerRadius.bottomLeft + spread};
          pushRectInstance(layer, cr, FillStyle::solid(c), StrokeStyle::none(), 1.f);
        }
      }
    }

    pushRectInstance(rect, cornerRadius, fill, stroke, state_.opacity);
  }

  void drawLine(Point from, Point to, StrokeStyle const& stroke) override {
    if (stroke.isNone()) return;
    Point a = state_.transform.apply(from);
    Point b = state_.transform.apply(to);
    if (!clipLineToCurrentClip(a, b)) return;
    StrokeStyle scaled = stroke;
    float const sx = std::hypot(state_.transform.m[0], state_.transform.m[1]);
    float const sy = std::hypot(state_.transform.m[3], state_.transform.m[4]);
    float const s = (sx > 0.f || sy > 0.f) ? (sx + sy) * 0.5f : 1.f;
    scaled.width *= s;
    Path path;
    path.moveTo(a);
    path.lineTo(b);
    DrawState saved = state_;
    state_.transform = Mat3::identity();
    appendPath(path, FillStyle::none(), scaled);
    state_ = saved;
  }
  void drawPath(Path const& path, FillStyle const& fill, StrokeStyle const& stroke, ShadowStyle const& shadow) override {
    if (!shadow.isNone()) {
      DrawState saved = state_;
      state_.transform = state_.transform * Mat3::translate(shadow.offset);
      appendPath(path, FillStyle::solid(shadow.color), StrokeStyle::none());
      state_ = saved;
    }
    appendPath(path, fill, stroke);
  }
  void drawCircle(Point center, float radius, FillStyle const& fill, StrokeStyle const& stroke) override {
    Rect r{center.x - radius, center.y - radius, radius * 2.f, radius * 2.f};
    drawRect(r, CornerRadius::pill(r), fill, stroke, ShadowStyle::none());
  }

  void drawTextLayout(TextLayout const& layout, Point origin) override {
    ensureAtlasDescriptor();
    std::uint32_t first = static_cast<std::uint32_t>(quads_.size());
    for (TextLayout::PlacedRun const& placed : layout.runs) {
      for (std::size_t i = 0; i < placed.run.glyphIds.size(); ++i) {
        GlyphSlot const* slot = glyphSlot(placed.run.fontId, placed.run.glyphIds[i], placed.run.fontSize);
        if (!slot || slot->w == 0 || slot->h == 0) continue;
        Point pos = origin + placed.origin + placed.run.positions[i];
        Rect glyphRect = Rect::sharp(pos.x + slot->bearing.x / dpiScaleX_,
                                     pos.y - slot->bearing.y / dpiScaleY_,
                                     static_cast<float>(slot->w) / dpiScaleX_,
                                     static_cast<float>(slot->h) / dpiScaleY_);
        Rect r = transformedBounds(glyphRect);
        Point p00 = state_.transform.apply({glyphRect.x, glyphRect.y});
        Point p11 = state_.transform.apply({glyphRect.x + glyphRect.width, glyphRect.y + glyphRect.height});
        QuadInstance q{};
        q.rect[0] = r.x; q.rect[1] = r.y;
        q.rect[2] = r.width; q.rect[3] = r.height;
        q.uv[0] = p00.x <= p11.x ? slot->u0 : slot->u1;
        q.uv[1] = p00.y <= p11.y ? slot->v0 : slot->v1;
        q.uv[2] = p00.x <= p11.x ? slot->u1 : slot->u0;
        q.uv[3] = p00.y <= p11.y ? slot->v1 : slot->v0;
        putColor(q.color, placed.run.color, state_.opacity);
        quads_.push_back(q);
      }
    }
    std::uint32_t count = static_cast<std::uint32_t>(quads_.size()) - first;
    if (count > 0) {
      batches_.push_back(ImageBatch{&atlas_, first, count});
      ops_.push_back(DrawOp{DrawOp::Kind::Image, &atlas_, first, count});
      debug::perf::recordDrawCall(debug::perf::RenderCounterKind::Glyph);
    }
  }

  void drawImage(Image const& image, Rect const& src, Rect const& dst, CornerRadius const&, float opacity) override {
    auto const* vi = dynamic_cast<VulkanImage const*>(&image);
    if (!vi || src.width <= 0.f || src.height <= 0.f || dst.width <= 0.f || dst.height <= 0.f) return;
    Texture* texture = ensureImageTexture(*vi);
    if (!texture) return;
    Rect r = transformedBounds(dst);
    Point p00 = state_.transform.apply({dst.x, dst.y});
    Point p11 = state_.transform.apply({dst.x + dst.width, dst.y + dst.height});
    Size sz = image.size();
    float const u0 = src.x / sz.width;
    float const v0 = src.y / sz.height;
    float const u1 = (src.x + src.width) / sz.width;
    float const v1 = (src.y + src.height) / sz.height;
    QuadInstance q{};
    q.rect[0] = r.x; q.rect[1] = r.y; q.rect[2] = r.width; q.rect[3] = r.height;
    q.uv[0] = p00.x <= p11.x ? u0 : u1;
    q.uv[1] = p00.y <= p11.y ? v0 : v1;
    q.uv[2] = p00.x <= p11.x ? u1 : u0;
    q.uv[3] = p00.y <= p11.y ? v1 : v0;
    q.color[0] = q.color[1] = q.color[2] = 1.f; q.color[3] = opacity * state_.opacity;
    std::uint32_t first = static_cast<std::uint32_t>(quads_.size());
    quads_.push_back(q);
    batches_.push_back(ImageBatch{texture, first, 1});
    ops_.push_back(DrawOp{DrawOp::Kind::Image, texture, first, 1});
    debug::perf::recordDrawCall(debug::perf::RenderCounterKind::Image);
  }

  void drawImageTiled(Image const& image, Rect const& dst, CornerRadius const& corners, float opacity) override {
    Size sz = image.size();
    drawImage(image, Rect::sharp(0.f, 0.f, sz.width, sz.height), dst, corners, opacity);
  }

  void* gpuDevice() const override { return device_; }

private:
  struct DrawState {
    Mat3 transform = Mat3::identity();
    Rect clip{};
    float opacity = 1.f;
    BlendMode blendMode = BlendMode::Normal;
  };

  Rect transformedBounds(Rect rect) const {
    std::array<Point, 4> pts{
        state_.transform.apply({rect.x, rect.y}),
        state_.transform.apply({rect.x + rect.width, rect.y}),
        state_.transform.apply({rect.x + rect.width, rect.y + rect.height}),
        state_.transform.apply({rect.x, rect.y + rect.height}),
    };
    float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
    for (Point p : pts) {
      minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
      minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
    }
    return Rect::sharp(minX, minY, maxX - minX, maxY - minY);
  }

  bool clipLineToCurrentClip(Point& a, Point& b) const {
    float t0 = 0.f;
    float t1 = 1.f;
    float const dx = b.x - a.x;
    float const dy = b.y - a.y;
    float const xMin = state_.clip.x;
    float const yMin = state_.clip.y;
    float const xMax = state_.clip.x + state_.clip.width;
    float const yMax = state_.clip.y + state_.clip.height;
    auto edge = [&](float p, float q) {
      if (std::abs(p) < 1e-6f) return q >= 0.f;
      float const r = q / p;
      if (p < 0.f) {
        if (r > t1) return false;
        if (r > t0) t0 = r;
      } else {
        if (r < t0) return false;
        if (r < t1) t1 = r;
      }
      return true;
    };
    if (!edge(-dx, a.x - xMin) || !edge(dx, xMax - a.x) ||
        !edge(-dy, a.y - yMin) || !edge(dy, yMax - a.y)) {
      return false;
    }
    Point const original = a;
    a = {original.x + dx * t0, original.y + dy * t0};
    b = {original.x + dx * t1, original.y + dy * t1};
    return state_.clip.width > 0.f && state_.clip.height > 0.f;
  }

  void encodeFill(FillStyle const& fill, RectInstance& inst) {
    Color c{};
    if (fill.solidColor(&c)) {
      putColor(inst.fill0, c, 1.f);
      inst.stops[0] = 0.f;
      inst.params[1] = 1.f;
      return;
    }
    auto writeStops = [&](auto const& g) {
      inst.params[1] = static_cast<float>(g.stopCount);
      for (std::uint8_t i = 0; i < g.stopCount && i < 4; ++i) {
        float* colors[] = {inst.fill0, inst.fill1, inst.fill2, inst.fill3};
        putColor(colors[i], g.stops[i].color, 1.f);
        inst.stops[i] = g.stops[i].position;
      }
    };
    LinearGradient lg{};
    if (fill.linearGradient(&lg) && lg.stopCount > 0) {
      inst.params[0] = 1.f;
      inst.gradient[0] = lg.start.x; inst.gradient[1] = lg.start.y;
      inst.gradient[2] = lg.end.x; inst.gradient[3] = lg.end.y;
      writeStops(lg);
      return;
    }
    RadialGradient rg{};
    if (fill.radialGradient(&rg) && rg.stopCount > 0) {
      inst.params[0] = 2.f;
      inst.gradient[0] = rg.center.x; inst.gradient[1] = rg.center.y; inst.gradient[2] = rg.radius;
      writeStops(rg);
      return;
    }
    ConicalGradient cg{};
    if (fill.conicalGradient(&cg) && cg.stopCount > 0) {
      inst.params[0] = 3.f;
      inst.gradient[0] = cg.center.x; inst.gradient[1] = cg.center.y; inst.gradient[2] = cg.startAngleRadians;
      writeStops(cg);
      return;
    }
    putColor(inst.fill0, Colors::transparent);
    inst.params[1] = 1.f;
  }

  void createInstance() {
    char const* exts[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME};
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "Flux";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &app;
    info.enabledExtensionCount = 2;
    info.ppEnabledExtensionNames = exts;
    vkCheck(vkCreateInstance(&info, nullptr, &instance_), "vkCreateInstance");
  }

  void pickDevice() {
    std::uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    if (!count) throw std::runtime_error("No Vulkan physical devices");
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());
    for (VkPhysicalDevice d : devices) {
      std::uint32_t familiesCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(d, &familiesCount, nullptr);
      std::vector<VkQueueFamilyProperties> families(familiesCount);
      vkGetPhysicalDeviceQueueFamilyProperties(d, &familiesCount, families.data());
      for (std::uint32_t i = 0; i < familiesCount; ++i) {
        VkBool32 present = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface_, &present);
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
          physical_ = d;
          queueFamily_ = i;
          return;
        }
      }
    }
    throw std::runtime_error("No Vulkan graphics/present queue");
  }

  void createDevice() {
    float priority = 1.f;
    VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    q.queueFamilyIndex = queueFamily_;
    q.queueCount = 1;
    q.pQueuePriorities = &priority;
    char const* exts[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &q;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = exts;
    vkCheck(vkCreateDevice(physical_, &info, nullptr, &device_), "vkCreateDevice");
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
  }

  void createCommandObjects() {
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = queueFamily_;
    vkCheck(vkCreateCommandPool(device_, &pool, nullptr, &commandPool_), "vkCreateCommandPool");
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = commandPool_;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    vkCheck(vkAllocateCommandBuffers(device_, &alloc, &commandBuffer_), "vkAllocateCommandBuffers");
    VkSemaphoreCreateInfo sem{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCheck(vkCreateSemaphore(device_, &sem, nullptr, &imageAvailable_), "vkCreateSemaphore");
    vkCheck(vkCreateSemaphore(device_, &sem, nullptr, &renderFinished_), "vkCreateSemaphore");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCheck(vkCreateFence(device_, &fence, nullptr, &frameFence_), "vkCreateFence");
  }

  void createDescriptors() {
    VkDescriptorPoolSize sizes[3]{
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8},
    };
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets = 512;
    pool.poolSizeCount = 3;
    pool.pPoolSizes = sizes;
    vkCheck(vkCreateDescriptorPool(device_, &pool, nullptr, &descriptorPool_), "vkCreateDescriptorPool");
    rectDescriptorLayout_ = createStorageLayout();
    quadDescriptorLayout_ = createStorageLayout();
    VkDescriptorSetLayoutBinding b{};
    b.binding = 0;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount = 1;
    layout.pBindings = &b;
    vkCheck(vkCreateDescriptorSetLayout(device_, &layout, nullptr, &textureDescriptorLayout_),
            "vkCreateDescriptorSetLayout");
  }

  VkDescriptorSetLayout createStorageLayout() {
    VkDescriptorSetLayoutBinding b{};
    b.binding = 0;
    b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    info.bindingCount = 1;
    info.pBindings = &b;
    VkDescriptorSetLayout out = VK_NULL_HANDLE;
    vkCheck(vkCreateDescriptorSetLayout(device_, &info, nullptr, &out), "vkCreateDescriptorSetLayout");
    return out;
  }

  void createSampler() {
    VkSamplerCreateInfo s{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    s.magFilter = VK_FILTER_LINEAR;
    s.minFilter = VK_FILTER_LINEAR;
    s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    s.addressModeU = s.addressModeV = s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCheck(vkCreateSampler(device_, &s, nullptr, &sampler_), "vkCreateSampler");
  }

  void createRenderPass() {
    VkAttachmentDescription color{};
    color.format = surfaceFormat_.format == VK_FORMAT_UNDEFINED ? VK_FORMAT_B8G8R8A8_UNORM : surfaceFormat_.format;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = 1;
    info.pAttachments = &color;
    info.subpassCount = 1;
    info.pSubpasses = &sub;
    vkCheck(vkCreateRenderPass(device_, &info, nullptr, &renderPass_), "vkCreateRenderPass");
  }

  void chooseSurfaceFormat() {
    std::uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_, surface_, &count, formats.data());
    surfaceFormat_ = formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}
                                     : formats[0];
    auto choose = [&](VkFormat format) {
      for (auto const& fmt : formats) {
        if (fmt.format == format) {
          surfaceFormat_ = fmt;
          return true;
        }
      }
      return false;
    };
    if (choose(VK_FORMAT_B8G8R8A8_UNORM) || choose(VK_FORMAT_R8G8B8A8_UNORM) ||
        choose(VK_FORMAT_A8B8G8R8_UNORM_PACK32)) {
      return;
    }
    for (auto const& fmt : formats) {
      if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB || fmt.format == VK_FORMAT_R8G8B8A8_SRGB ||
          fmt.format == VK_FORMAT_A8B8G8R8_SRGB_PACK32) {
        surfaceFormat_ = fmt;
        break;
      }
    }
  }

  struct VertexInput {
    VkVertexInputBindingDescription const* bindings = nullptr;
    std::uint32_t bindingCount = 0;
    VkVertexInputAttributeDescription const* attrs = nullptr;
    std::uint32_t attrCount = 0;
  };

  void createPipelines() {
    rectPipelineLayout_ = createPipelineLayout({rectDescriptorLayout_}, true);
    imagePipelineLayout_ = createPipelineLayout({quadDescriptorLayout_, textureDescriptorLayout_}, true);
    pathPipelineLayout_ = createPipelineLayout({}, false);
    rectPipeline_ = createPipeline(rectPipelineLayout_, _tmp_rect_vert_spv, _tmp_rect_vert_spv_len,
                                   _tmp_rect_frag_spv, _tmp_rect_frag_spv_len, {});
    imagePipeline_ = createPipeline(imagePipelineLayout_, _tmp_image_vert_spv, _tmp_image_vert_spv_len,
                                    _tmp_image_frag_spv, _tmp_image_frag_spv_len, {});
    std::array<VkVertexInputBindingDescription, 1> binding{};
    binding[0].binding = 0;
    binding[0].stride = sizeof(PathVertex);
    binding[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::array<VkVertexInputAttributeDescription, 3> attrs{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(PathVertex, x)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(PathVertex, color)};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(PathVertex, viewport)};
    pathPipeline_ = createPipeline(pathPipelineLayout_, _tmp_path_vert_spv, _tmp_path_vert_spv_len,
                                   _tmp_path_frag_spv, _tmp_path_frag_spv_len,
                                   {binding.data(), 1, attrs.data(), static_cast<std::uint32_t>(attrs.size())});
  }

  VkPipelineLayout createPipelineLayout(std::initializer_list<VkDescriptorSetLayout> layouts, bool viewportPush) {
    std::vector<VkDescriptorSetLayout> setLayouts(layouts);
    VkPushConstantRange push{};
    push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push.offset = 0;
    push.size = sizeof(float) * 2;
    VkPipelineLayoutCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    info.setLayoutCount = static_cast<std::uint32_t>(setLayouts.size());
    info.pSetLayouts = setLayouts.data();
    if (viewportPush) {
      info.pushConstantRangeCount = 1;
      info.pPushConstantRanges = &push;
    }
    VkPipelineLayout out = VK_NULL_HANDLE;
    vkCheck(vkCreatePipelineLayout(device_, &info, nullptr, &out), "vkCreatePipelineLayout");
    return out;
  }

  VkPipeline createPipeline(VkPipelineLayout layout, unsigned char const* vertBytes, unsigned int vertLen,
                            unsigned char const* fragBytes, unsigned int fragLen, VertexInput input) {
    VkShaderModule vert = shaderModule(device_, vertBytes, vertLen);
    VkShaderModule frag = shaderModule(device_, fragBytes, fragLen);
    VkPipelineShaderStageCreateInfo stages[2]{{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                              {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = input.bindingCount;
    vi.pVertexBindingDescriptions = input.bindings;
    vi.vertexAttributeDescriptionCount = input.attrCount;
    vi.pVertexAttributeDescriptions = input.attrs;
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                           VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &blend;
    VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = states;
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vi;
    info.pInputAssemblyState = &ia;
    info.pViewportState = &vp;
    info.pRasterizationState = &rs;
    info.pMultisampleState = &ms;
    info.pColorBlendState = &cb;
    info.pDynamicState = &dyn;
    info.layout = layout;
    info.renderPass = renderPass_;
    VkPipeline out = VK_NULL_HANDLE;
    vkCheck(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &out),
            "vkCreateGraphicsPipelines");
    vkDestroyShaderModule(device_, vert, nullptr);
    vkDestroyShaderModule(device_, frag, nullptr);
    return out;
  }

  void recreateSwapchain() {
    if (!device_) return;
    if (frameFence_) {
      vkWaitForFences(device_, 1, &frameFence_, VK_TRUE, UINT64_MAX);
    }
    VkSwapchainKHR oldSwapchain = swapchain_;
    std::vector<VkImageView> oldViews = std::move(swapchainViews_);
    std::vector<VkFramebuffer> oldFramebuffers = std::move(framebuffers_);
    swapchain_ = VK_NULL_HANDLE;
    swapchainImages_.clear();
    swapchainViews_.clear();
    framebuffers_.clear();
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_, surface_, &caps);
    std::uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &presentCount, nullptr);
    std::vector<VkPresentModeKHR> modes(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_, surface_, &presentCount, modes.data());
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : modes) {
      if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
        mode = m;
        break;
      }
    }
    swapExtent_ = caps.currentExtent.width != UINT32_MAX
                      ? caps.currentExtent
                      : VkExtent2D{static_cast<std::uint32_t>(std::max(1, framebufferWidth_)),
                                   static_cast<std::uint32_t>(std::max(1, framebufferHeight_))};
    framebufferWidth_ = static_cast<int>(std::max(1u, swapExtent_.width));
    framebufferHeight_ = static_cast<int>(std::max(1u, swapExtent_.height));
    std::uint32_t imageCount = std::clamp(caps.minImageCount + 1, caps.minImageCount,
                                          caps.maxImageCount ? caps.maxImageCount : caps.minImageCount + 1);
    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface_;
    info.minImageCount = imageCount;
    info.imageFormat = surfaceFormat_.format;
    info.imageColorSpace = surfaceFormat_.colorSpace;
    info.imageExtent = swapExtent_;
    info.imageArrayLayers = 1;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = mode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = oldSwapchain;
    vkCheck(vkCreateSwapchainKHR(device_, &info, nullptr, &swapchain_), "vkCreateSwapchainKHR");
    std::uint32_t count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &count, nullptr);
    swapchainImages_.resize(count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &count, swapchainImages_.data());
    swapchainViews_.resize(count);
    framebuffers_.resize(count);
    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
      VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
      view.image = swapchainImages_[i];
      view.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view.format = surfaceFormat_.format;
      view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view.subresourceRange.levelCount = 1;
      view.subresourceRange.layerCount = 1;
      vkCheck(vkCreateImageView(device_, &view, nullptr, &swapchainViews_[i]), "vkCreateImageView");
      VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
      fb.renderPass = renderPass_;
      fb.attachmentCount = 1;
      fb.pAttachments = &swapchainViews_[i];
      fb.width = swapExtent_.width;
      fb.height = swapExtent_.height;
      fb.layers = 1;
      vkCheck(vkCreateFramebuffer(device_, &fb, nullptr, &framebuffers_[i]), "vkCreateFramebuffer");
    }
    for (VkFramebuffer fb : oldFramebuffers) vkDestroyFramebuffer(device_, fb, nullptr);
    for (VkImageView view : oldViews) vkDestroyImageView(device_, view, nullptr);
    if (oldSwapchain) vkDestroySwapchainKHR(device_, oldSwapchain, nullptr);
    swapchainDirty_ = false;
  }

  void destroySwapchain() {
    for (VkFramebuffer fb : framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();
    for (VkImageView view : swapchainViews_) vkDestroyImageView(device_, view, nullptr);
    swapchainViews_.clear();
    swapchainImages_.clear();
    if (swapchain_) {
      vkDestroySwapchainKHR(device_, swapchain_, nullptr);
      swapchain_ = VK_NULL_HANDLE;
    }
  }

  void createAtlas() {
    atlas_.width = 2048;
    atlas_.height = 2048;
    atlasPixels_.assign(static_cast<std::size_t>(atlas_.width) * atlas_.height, Rgba{255, 255, 255, 0});
    createTexture(atlas_, atlas_.width, atlas_.height, atlasPixels_.data(), false);
    ensureTextureDescriptor(atlas_);
  }

  void appendPath(Path const& path, FillStyle const& fill, StrokeStyle const& stroke) {
    if (path.isEmpty()) return;
    auto subpaths = PathFlattener::flattenSubpaths(path);
    if (subpaths.empty()) return;
    std::uint32_t const firstVertex = static_cast<std::uint32_t>(pathVerts_.size());
    for (auto& sp : subpaths) {
      for (Point& p : sp) p = state_.transform.apply(p);
    }
    Rect const bounds = boundsOfSubpaths(subpaths);
    auto append = [&](TessellatedPath&& tess, FillStyle const* gradientSource = nullptr) {
      if (gradientSource) applyGradientFill(tess, *gradientSource, bounds, state_.opacity);
      pathVerts_.insert(pathVerts_.end(), tess.vertices.begin(), tess.vertices.end());
    };
    if (!fill.isNone()) {
      Color fc{};
      if (representativeFillColor(fill, &fc)) {
        fc.a *= state_.opacity;
        std::vector<std::vector<Point>> nonempty;
        for (auto const& sp : subpaths) {
          if (sp.size() >= 3) nonempty.push_back(sp);
        }
        if (!nonempty.empty()) {
          append(PathFlattener::tessellateFillContours(nonempty, fc, static_cast<float>(width_),
                                                       static_cast<float>(height_),
                                                       PathFlattener::tessWindingFromFillRule(fill.fillRule)),
                 &fill);
        }
      }
    }
    if (!stroke.isNone()) {
      Color sc{};
      if (stroke.solidColor(&sc)) {
        sc.a *= state_.opacity;
        float const sx = std::hypot(state_.transform.m[0], state_.transform.m[1]);
        float const sy = std::hypot(state_.transform.m[3], state_.transform.m[4]);
        float const s = (sx > 0.f || sy > 0.f) ? (sx + sy) * 0.5f : 1.f;
        for (auto const& sp : subpaths) {
          if (sp.size() >= 2) {
            append(PathFlattener::tessellateStroke(sp, stroke.width * s, sc, static_cast<float>(width_),
                                                   static_cast<float>(height_), stroke.join, stroke.cap));
          }
        }
      }
    }
    std::uint32_t const vertexCount = static_cast<std::uint32_t>(pathVerts_.size()) - firstVertex;
    if (vertexCount > 0) {
      ops_.push_back(DrawOp{DrawOp::Kind::Path, nullptr, firstVertex, vertexCount});
    }
  }

  void uploadFrameBuffers() {
    ensureBuffer(rectBuffer_, std::max<VkDeviceSize>(sizeof(RectInstance), rects_.size() * sizeof(RectInstance)),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    upload(rectBuffer_, rects_.data(), rects_.size() * sizeof(RectInstance));
    ensureStorageDescriptor(rectDescriptorSet_, rectDescriptorLayout_, rectBuffer_);
    ensureBuffer(quadBuffer_, std::max<VkDeviceSize>(sizeof(QuadInstance), quads_.size() * sizeof(QuadInstance)),
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    upload(quadBuffer_, quads_.data(), quads_.size() * sizeof(QuadInstance));
    ensureStorageDescriptor(quadDescriptorSet_, quadDescriptorLayout_, quadBuffer_);
    ensureBuffer(pathBuffer_, std::max<VkDeviceSize>(sizeof(PathVertex), pathVerts_.size() * sizeof(PathVertex)),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    upload(pathBuffer_, pathVerts_.data(), pathVerts_.size() * sizeof(PathVertex));
  }

  void ensureBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage) {
    if (buffer.buffer && buffer.capacity >= size) return;
    destroyBuffer(buffer);
    buffer.capacity = std::max<VkDeviceSize>(size, 1024);
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = buffer.capacity;
    info.usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(vkCreateBuffer(device_, &info, nullptr, &buffer.buffer), "vkCreateBuffer");
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device_, buffer.buffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(physical_, req.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkCheck(vkAllocateMemory(device_, &alloc, nullptr, &buffer.memory), "vkAllocateMemory");
    vkBindBufferMemory(device_, buffer.buffer, buffer.memory, 0);
  }

  void upload(Buffer& buffer, void const* data, std::size_t size) {
    if (!size) return;
    void* mapped = nullptr;
    vkMapMemory(device_, buffer.memory, 0, size, 0, &mapped);
    std::memcpy(mapped, data, size);
    vkUnmapMemory(device_, buffer.memory);
  }

  void destroyBuffer(Buffer& buffer) {
    if (buffer.buffer) vkDestroyBuffer(device_, buffer.buffer, nullptr);
    if (buffer.memory) vkFreeMemory(device_, buffer.memory, nullptr);
    buffer = {};
  }

  void ensureStorageDescriptor(VkDescriptorSet& set, VkDescriptorSetLayout layout, Buffer const& buffer) {
    if (!set) {
      VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
      alloc.descriptorPool = descriptorPool_;
      alloc.descriptorSetCount = 1;
      alloc.pSetLayouts = &layout;
      vkCheck(vkAllocateDescriptorSets(device_, &alloc, &set), "vkAllocateDescriptorSets");
    }
    VkDescriptorBufferInfo bi{buffer.buffer, 0, buffer.capacity};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bi;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  }

  void drawRectRange(std::uint32_t first, std::uint32_t count) {
    if (count == 0) return;
    setViewportScissor();
    float viewport[2] = {static_cast<float>(width_), static_cast<float>(height_)};
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, rectPipeline_);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, rectPipelineLayout_, 0, 1,
                            &rectDescriptorSet_, 0, nullptr);
    vkCmdPushConstants(commandBuffer_, rectPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(viewport), viewport);
    vkCmdDraw(commandBuffer_, 6, count, 0, first);
  }

  void drawPathRange(std::uint32_t first, std::uint32_t count) {
    if (count == 0) return;
    setViewportScissor();
    VkDeviceSize offset = 0;
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pathPipeline_);
    vkCmdBindVertexBuffers(commandBuffer_, 0, 1, &pathBuffer_.buffer, &offset);
    vkCmdDraw(commandBuffer_, count, 1, first, 0);
  }

  void drawImageRange(Texture* texture, std::uint32_t first, std::uint32_t count) {
    if (!texture || !texture->descriptor || count == 0) return;
    setViewportScissor();
    float viewport[2] = {static_cast<float>(width_), static_cast<float>(height_)};
    vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipeline_);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipelineLayout_, 0, 1,
                            &quadDescriptorSet_, 0, nullptr);
    vkCmdPushConstants(commandBuffer_, imagePipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(viewport), viewport);
    vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipelineLayout_, 1, 1,
                            &texture->descriptor, 0, nullptr);
    vkCmdDraw(commandBuffer_, 6, count, 0, first);
  }

  void drawOps() {
    for (DrawOp const& op : ops_) {
      switch (op.kind) {
      case DrawOp::Kind::Rect:
        drawRectRange(op.first, op.count);
        break;
      case DrawOp::Kind::Path:
        drawPathRange(op.first, op.count);
        break;
      case DrawOp::Kind::Image:
        drawImageRange(op.texture, op.first, op.count);
        break;
      }
    }
  }

  void setViewportScissor() {
    VkViewport vp{0.f, 0.f, static_cast<float>(swapExtent_.width), static_cast<float>(swapExtent_.height), 0.f, 1.f};
    VkRect2D sc{{0, 0}, swapExtent_};
    vkCmdSetViewport(commandBuffer_, 0, 1, &vp);
    vkCmdSetScissor(commandBuffer_, 0, 1, &sc);
  }

  GlyphSlot const* glyphSlot(std::uint32_t fontId, std::uint16_t glyphId, float fontSize) {
    std::uint16_t size = static_cast<std::uint16_t>(std::clamp(std::round(fontSize * dpiScaleY_), 1.f, 512.f));
    GlyphKey key{fontId, glyphId, size};
    auto it = glyphs_.find(key);
    if (it != glyphs_.end()) return &it->second;
    std::uint32_t gw = 0, gh = 0;
    Point bearing{};
    std::vector<std::uint8_t> alpha = textSystem_.rasterizeGlyph(fontId, glyphId, static_cast<float>(size), gw, gh,
                                                                 bearing);
    if (gw == 0 || gh == 0 || alpha.empty()) {
      auto [inserted, ok] = glyphs_.emplace(key, GlyphSlot{});
      (void)ok;
      return &inserted->second;
    }
    int const pad = 1;
    if (atlasX_ + static_cast<int>(gw) + pad >= atlas_.width) {
      atlasX_ = pad;
      atlasY_ += atlasRowH_ + pad;
      atlasRowH_ = 0;
    }
    if (atlasY_ + static_cast<int>(gh) + pad >= atlas_.height) return nullptr;
    int const x = atlasX_;
    int const y = atlasY_;
    atlasX_ += static_cast<int>(gw) + pad;
    atlasRowH_ = std::max(atlasRowH_, static_cast<int>(gh));
    for (std::uint32_t row = 0; row < gh; ++row) {
      for (std::uint32_t col = 0; col < gw; ++col) {
        Rgba& px = atlasPixels_[static_cast<std::size_t>(y + row) * atlas_.width + x + col];
        px = {255, 255, 255, alpha[static_cast<std::size_t>(row) * gw + col]};
      }
    }
    atlasDirty_ = true;
    GlyphSlot slot{};
    slot.u0 = static_cast<float>(x) / atlas_.width;
    slot.v0 = static_cast<float>(y) / atlas_.height;
    slot.u1 = static_cast<float>(x + static_cast<int>(gw)) / atlas_.width;
    slot.v1 = static_cast<float>(y + static_cast<int>(gh)) / atlas_.height;
    slot.w = gw;
    slot.h = gh;
    slot.bearing = bearing;
    auto [inserted, ok] = glyphs_.emplace(key, slot);
    (void)ok;
    return &inserted->second;
  }

  void ensureAtlasDescriptor() { ensureTextureDescriptor(atlas_); }

  void uploadAtlasIfNeeded() {
    if (!atlasDirty_) return;
    uploadTexture(atlas_, atlasPixels_.data());
    atlasDirty_ = false;
  }

  Texture* ensureImageTexture(VulkanImage const& image) {
    auto it = imageTextures_.find(&image);
    if (it != imageTextures_.end()) return &it->second;
    Texture tex{};
    createTexture(tex, image.width, image.height, image.pixels.data(), true);
    ensureTextureDescriptor(tex);
    auto [inserted, ok] = imageTextures_.emplace(&image, std::move(tex));
    (void)ok;
    return &inserted->second;
  }

  void createTexture(Texture& tex, int width, int height, Rgba const* pixels, bool uploadNow) {
    tex.width = width;
    tex.height = height;
    VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = VK_FORMAT_R8G8B8A8_UNORM;
    image.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    vkCheck(vkCreateImage(device_, &image, nullptr, &tex.image), "vkCreateImage");
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device_, tex.image, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(physical_, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkCheck(vkAllocateMemory(device_, &alloc, nullptr, &tex.memory), "vkAllocateMemory");
    vkBindImageMemory(device_, tex.image, tex.memory, 0);
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = tex.image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = VK_FORMAT_R8G8B8A8_UNORM;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    vkCheck(vkCreateImageView(device_, &view, nullptr, &tex.view), "vkCreateImageView");
    transitionImmediate(tex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (uploadNow) uploadTexture(tex, pixels);
  }

  void uploadTexture(Texture& tex, Rgba const* pixels) {
    if (!pixels || tex.width <= 0 || tex.height <= 0) return;
    Buffer staging{};
    VkDeviceSize size = static_cast<VkDeviceSize>(tex.width) * tex.height * sizeof(Rgba);
    ensureBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    upload(staging, pixels, static_cast<std::size_t>(size));
    VkCommandBuffer cmd = beginImmediate();
    transition(cmd, tex.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {static_cast<std::uint32_t>(tex.width), static_cast<std::uint32_t>(tex.height), 1};
    vkCmdCopyBufferToImage(cmd, staging.buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    transition(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    endImmediate(cmd);
    destroyBuffer(staging);
  }

  void ensureTextureDescriptor(Texture& tex) {
    if (tex.descriptor) return;
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool = descriptorPool_;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &textureDescriptorLayout_;
    vkCheck(vkAllocateDescriptorSets(device_, &alloc, &tex.descriptor), "vkAllocateDescriptorSets");
    VkDescriptorImageInfo ii{sampler_, tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = tex.descriptor;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &ii;
    vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  }

  void destroyTexture(Texture& tex) {
    if (tex.view) vkDestroyImageView(device_, tex.view, nullptr);
    if (tex.image) vkDestroyImage(device_, tex.image, nullptr);
    if (tex.memory) vkFreeMemory(device_, tex.memory, nullptr);
    tex = {};
  }

  VkCommandBuffer beginImmediate() {
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = commandPool_;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkCheck(vkAllocateCommandBuffers(device_, &alloc, &cmd), "vkAllocateCommandBuffers");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(cmd, &begin), "vkBeginCommandBuffer");
    return cmd;
  }

  void endImmediate(VkCommandBuffer cmd) {
    vkCheck(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkCheck(vkQueueSubmit(queue_, 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit");
    vkQueueWaitIdle(queue_);
    vkFreeCommandBuffers(device_, commandPool_, 1, &cmd);
  }

  void transitionImmediate(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer cmd = beginImmediate();
    transition(cmd, image, oldLayout, newLayout);
    endImmediate(cmd);
  }

  void transition(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    b.srcAccessMask = oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
    b.dstAccessMask = newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                          ? VK_ACCESS_TRANSFER_WRITE_BIT
                          : (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? VK_ACCESS_TRANSFER_READ_BIT : 0);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &b);
  }

  void writeDebugScreenshotIfRequested(VkImage source) {
    if (debugScreenshotWritten_) return;
    char const* path = std::getenv("FLUX_DEBUG_SCREENSHOT_PATH");
    if (!path || !*path) return;
    Buffer staging{};
    VkDeviceSize size = static_cast<VkDeviceSize>(framebufferWidth_) * framebufferHeight_ * 4u;
    ensureBuffer(staging, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {static_cast<std::uint32_t>(framebufferWidth_),
                        static_cast<std::uint32_t>(framebufferHeight_), 1};
    vkCmdCopyImageToBuffer(commandBuffer_, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copy);
    pendingScreenshotPath_ = path;
    pendingScreenshotBuffer_ = staging;
    pendingScreenshotSize_ = size;
    debugScreenshotWritten_ = true;
  }

  void flushScreenshot() {
    if (!pendingScreenshotBuffer_.buffer || pendingScreenshotPath_.empty()) return;
    vkDeviceWaitIdle(device_);
    void* mapped = nullptr;
    vkMapMemory(device_, pendingScreenshotBuffer_.memory, 0, pendingScreenshotSize_, 0, &mapped);
    std::ofstream out(pendingScreenshotPath_, std::ios::binary);
    out << "P6\n" << framebufferWidth_ << " " << framebufferHeight_ << "\n255\n";
    auto* bytes = static_cast<std::uint8_t const*>(mapped);
    bool const bgra = surfaceFormat_.format == VK_FORMAT_B8G8R8A8_UNORM ||
                      surfaceFormat_.format == VK_FORMAT_B8G8R8A8_SRGB;
    for (int i = 0; i < framebufferWidth_ * framebufferHeight_; ++i) {
      out.put(static_cast<char>(bytes[i * 4 + (bgra ? 2 : 0)]));
      out.put(static_cast<char>(bytes[i * 4 + 1]));
      out.put(static_cast<char>(bytes[i * 4 + (bgra ? 0 : 2)]));
    }
    vkUnmapMemory(device_, pendingScreenshotBuffer_.memory);
    destroyBuffer(pendingScreenshotBuffer_);
    pendingScreenshotPath_.clear();
  }

  wl_display* display_ = nullptr;
  wl_surface* wlSurface_ = nullptr;
  unsigned int handle_ = 0;
  TextSystem& textSystem_;
  int width_ = 1;
  int height_ = 1;
  int framebufferWidth_ = 1;
  int framebufferHeight_ = 1;
  float dpiScaleX_ = 1.f;
  float dpiScaleY_ = 1.f;
  Color clearColor_ = Colors::transparent;
  DrawState state_{};
  std::vector<DrawState> stateStack_;
  std::vector<RectInstance> rects_;
  std::vector<QuadInstance> quads_;
  std::vector<ImageBatch> batches_;
  std::vector<DrawOp> ops_;
  std::vector<PathVertex> pathVerts_;

  VkInstance instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  std::uint32_t queueFamily_ = 0;
  VkCommandPool commandPool_ = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;
  VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
  VkSemaphore renderFinished_ = VK_NULL_HANDLE;
  VkFence frameFence_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surfaceFormat_{};
  VkExtent2D swapExtent_{};
  std::vector<VkImage> swapchainImages_;
  std::vector<VkImageView> swapchainViews_;
  std::vector<VkFramebuffer> framebuffers_;
  VkRenderPass renderPass_ = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout rectDescriptorLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout quadDescriptorLayout_ = VK_NULL_HANDLE;
  VkDescriptorSetLayout textureDescriptorLayout_ = VK_NULL_HANDLE;
  VkDescriptorSet rectDescriptorSet_ = VK_NULL_HANDLE;
  VkDescriptorSet quadDescriptorSet_ = VK_NULL_HANDLE;
  VkSampler sampler_ = VK_NULL_HANDLE;
  VkPipelineLayout rectPipelineLayout_ = VK_NULL_HANDLE;
  VkPipelineLayout imagePipelineLayout_ = VK_NULL_HANDLE;
  VkPipelineLayout pathPipelineLayout_ = VK_NULL_HANDLE;
  VkPipeline rectPipeline_ = VK_NULL_HANDLE;
  VkPipeline imagePipeline_ = VK_NULL_HANDLE;
  VkPipeline pathPipeline_ = VK_NULL_HANDLE;
  Buffer rectBuffer_;
  Buffer quadBuffer_;
  Buffer pathBuffer_;
  Buffer pendingScreenshotBuffer_;
  VkDeviceSize pendingScreenshotSize_ = 0;
  std::string pendingScreenshotPath_;
  Texture atlas_;
  std::vector<Rgba> atlasPixels_;
  int atlasX_ = 1;
  int atlasY_ = 1;
  int atlasRowH_ = 0;
  bool atlasDirty_ = false;
  bool debugScreenshotWritten_ = false;
  bool swapchainDirty_ = true;
  std::unordered_map<GlyphKey, GlyphSlot, GlyphKeyHash> glyphs_;
  std::unordered_map<VulkanImage const*, Texture> imageTextures_;
};

std::unique_ptr<Canvas> createVulkanCanvas(wl_display* display, wl_surface* surface,
                                           unsigned int handle, TextSystem& textSystem) {
  return std::make_unique<VulkanCanvas>(display, surface, handle, textSystem);
}

std::shared_ptr<Image> loadImageFromFile(std::string_view path, void*) {
  using WebPGetInfoFn = int (*)(std::uint8_t const*, std::size_t, int*, int*);
  using WebPDecodeRGBAFn = std::uint8_t* (*)(std::uint8_t const*, std::size_t, int*, int*);
  using WebPFreeFn = void (*)(void*);

  std::ifstream in(std::filesystem::path(std::string(path)), std::ios::binary);
  if (!in) return nullptr;
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (data.empty()) return nullptr;
  void* lib = dlopen("libwebp.so.7", RTLD_LAZY | RTLD_LOCAL);
  if (!lib) lib = dlopen("libwebp.so", RTLD_LAZY | RTLD_LOCAL);
  if (!lib) return nullptr;
  auto getInfo = reinterpret_cast<WebPGetInfoFn>(dlsym(lib, "WebPGetInfo"));
  auto decode = reinterpret_cast<WebPDecodeRGBAFn>(dlsym(lib, "WebPDecodeRGBA"));
  auto freeWebP = reinterpret_cast<WebPFreeFn>(dlsym(lib, "WebPFree"));
  if (!getInfo || !decode || !freeWebP) {
    dlclose(lib);
    return nullptr;
  }
  int width = 0, height = 0;
  if (!getInfo(data.data(), data.size(), &width, &height) || width <= 0 || height <= 0) {
    dlclose(lib);
    return nullptr;
  }
  int decodedWidth = 0, decodedHeight = 0;
  std::uint8_t* decoded = decode(data.data(), data.size(), &decodedWidth, &decodedHeight);
  if (!decoded || decodedWidth != width || decodedHeight != height) {
    if (decoded) freeWebP(decoded);
    dlclose(lib);
    return nullptr;
  }
  std::vector<Rgba> rgba(static_cast<std::size_t>(width) * height);
  std::memcpy(rgba.data(), decoded, rgba.size() * sizeof(Rgba));
  freeWebP(decoded);
  dlclose(lib);
  return std::make_shared<VulkanImage>(width, height, std::move(rgba));
}

std::shared_ptr<Image> rasterizeToImage(Canvas&, Size, RasterizeDrawCallback, float) {
  return nullptr;
}

} // namespace flux
