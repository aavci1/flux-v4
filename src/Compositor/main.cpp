#include <Flux/Core/Color.hpp>
#include <Flux/Core/Geometry.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/Graphics/VulkanContext.hpp>
#include <Flux/Platform/Linux/KmsOutput.hpp>

#include "Compositor/WaylandServer.hpp"
#include "Graphics/Linux/FreeTypeTextSystem.hpp"
#include "Graphics/Vulkan/VulkanCanvas.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <unistd.h>
#include <vulkan/vulkan.h>

namespace {

std::atomic<bool> gRunning{true};

void onSignal(int) {
  gRunning.store(false, std::memory_order_relaxed);
}

struct CachedClientImage {
  std::uint64_t id = 0;
  std::uint64_t serial = 0;
  std::shared_ptr<flux::Image> image;
  bool logged = false;
};

struct SurfaceVisualState {
  std::chrono::steady_clock::time_point firstSeen{};
};

float clamp01(float value) {
  return std::clamp(value, 0.f, 1.f);
}

float easeOutCubic(float value) {
  float const t = clamp01(value);
  float const inverse = 1.f - t;
  return 1.f - inverse * inverse * inverse;
}

void updateCachedImage(flux::compositor::WaylandServer& wayland,
                       flux::Canvas& canvas,
                       flux::compositor::CommittedSurfaceSnapshot const& surface,
                       CachedClientImage& cached) {
  if (cached.image && cached.id == surface.id && cached.serial == surface.serial) return;

  cached.id = surface.id;
  cached.serial = surface.serial;
  cached.image.reset();
  cached.logged = false;
  std::int32_t const bufferWidth = surface.bufferWidth > 0 ? surface.bufferWidth : surface.width;
  std::int32_t const bufferHeight = surface.bufferHeight > 0 ? surface.bufferHeight : surface.height;
  if (!surface.rgbaPixels.empty()) {
    cached.image = flux::Image::fromRgbaPixels(static_cast<std::uint32_t>(bufferWidth),
                                               static_cast<std::uint32_t>(bufferHeight),
                                               surface.rgbaPixels,
                                               canvas.gpuDevice());
  } else if (!surface.dmabufPlanes.empty()) {
    std::vector<std::uint8_t> fallbackPixels;
    std::vector<int> fds = wayland.duplicateDmabufFds(surface.id);
    if (fds.size() == surface.dmabufPlanes.size()) {
      std::vector<flux::Image::DmabufPlane> planes;
      planes.reserve(surface.dmabufPlanes.size());
      for (std::size_t i = 0; i < surface.dmabufPlanes.size(); ++i) {
        planes.push_back({
            .fd = fds[i],
            .offset = surface.dmabufPlanes[i].offset,
            .stride = surface.dmabufPlanes[i].stride,
            .modifier = surface.dmabufPlanes[i].modifier,
        });
      }
      try {
        cached.image = flux::Image::fromDmabuf({
            .width = static_cast<std::uint32_t>(bufferWidth),
            .height = static_cast<std::uint32_t>(bufferHeight),
            .drmFormat = surface.dmabufFormat,
            .planes = planes,
        });
        if (cached.image && !cached.logged) {
          std::fprintf(stderr, "flux-compositor: imported DMABUF as Vulkan image\n");
        }
      } catch (std::exception const& e) {
        std::fprintf(stderr, "flux-compositor: dmabuf import failed: %s\n", e.what());
      }
    } else {
      for (int fd : fds) close(fd);
    }
    if (wayland.copyDmabufToRgba(surface.id, fallbackPixels)) {
      cached.image = flux::Image::fromRgbaPixels(static_cast<std::uint32_t>(bufferWidth),
                                                 static_cast<std::uint32_t>(bufferHeight),
                                                 fallbackPixels,
                                                 canvas.gpuDevice());
      if (cached.image && !cached.logged) {
        std::fprintf(stderr, "flux-compositor: displaying readable DMABUF contents\n");
        cached.logged = true;
      }
    }
  }
}

std::uint32_t monotonicMilliseconds() {
  using Clock = std::chrono::steady_clock;
  auto const now = std::chrono::duration_cast<std::chrono::milliseconds>(
      Clock::now().time_since_epoch());
  return static_cast<std::uint32_t>(now.count());
}

bool debugCompositorInput() {
  char const* value = std::getenv("FLUX_DEBUG_COMPOSITOR_INPUT");
  return value && *value && std::strcmp(value, "0") != 0;
}

void drawArrowCursor(flux::Canvas& canvas, float cursorX, float cursorY) {
  flux::Path cursor;
  cursor.moveTo({cursorX, cursorY});
  cursor.lineTo({cursorX + 2.f, cursorY + 22.f});
  cursor.lineTo({cursorX + 8.f, cursorY + 16.f});
  cursor.lineTo({cursorX + 14.f, cursorY + 30.f});
  cursor.lineTo({cursorX + 19.f, cursorY + 28.f});
  cursor.lineTo({cursorX + 13.f, cursorY + 14.f});
  cursor.lineTo({cursorX + 21.f, cursorY + 14.f});
  cursor.close();
  canvas.drawPath(cursor,
                  flux::FillStyle::solid(flux::Colors::white),
                  flux::StrokeStyle::solid(flux::Colors::black, 1.f));
}

void drawLineCursor(flux::Canvas& canvas, flux::Point from, flux::Point to, float width = 2.f) {
  canvas.drawLine({from.x + 1.f, from.y + 1.f},
                  {to.x + 1.f, to.y + 1.f},
                  flux::StrokeStyle::solid(flux::Colors::black, width + 1.f));
  canvas.drawLine(from, to, flux::StrokeStyle::solid(flux::Colors::white, width));
}

void drawFallbackCursor(flux::Canvas& canvas, flux::compositor::CursorShape shape, float cursorX, float cursorY) {
  switch (shape) {
  case flux::compositor::CursorShape::IBeam:
    drawLineCursor(canvas, {cursorX, cursorY}, {cursorX, cursorY + 24.f}, 2.f);
    drawLineCursor(canvas, {cursorX - 5.f, cursorY}, {cursorX + 5.f, cursorY}, 2.f);
    drawLineCursor(canvas, {cursorX - 5.f, cursorY + 24.f}, {cursorX + 5.f, cursorY + 24.f}, 2.f);
    return;
  case flux::compositor::CursorShape::Crosshair:
    drawLineCursor(canvas, {cursorX - 12.f, cursorY}, {cursorX + 12.f, cursorY}, 2.f);
    drawLineCursor(canvas, {cursorX, cursorY - 12.f}, {cursorX, cursorY + 12.f}, 2.f);
    return;
  case flux::compositor::CursorShape::Hand:
    canvas.drawCircle({cursorX + 7.f, cursorY + 8.f},
                      7.f,
                      flux::FillStyle::solid(flux::Colors::white),
                      flux::StrokeStyle::solid(flux::Colors::black, 1.f));
    drawLineCursor(canvas, {cursorX + 7.f, cursorY + 8.f}, {cursorX + 7.f, cursorY + 25.f}, 3.f);
    return;
  case flux::compositor::CursorShape::ResizeEW:
    drawLineCursor(canvas, {cursorX - 12.f, cursorY}, {cursorX + 12.f, cursorY}, 2.f);
    drawLineCursor(canvas, {cursorX - 12.f, cursorY}, {cursorX - 6.f, cursorY - 6.f}, 2.f);
    drawLineCursor(canvas, {cursorX - 12.f, cursorY}, {cursorX - 6.f, cursorY + 6.f}, 2.f);
    drawLineCursor(canvas, {cursorX + 12.f, cursorY}, {cursorX + 6.f, cursorY - 6.f}, 2.f);
    drawLineCursor(canvas, {cursorX + 12.f, cursorY}, {cursorX + 6.f, cursorY + 6.f}, 2.f);
    return;
  case flux::compositor::CursorShape::ResizeNS:
    drawLineCursor(canvas, {cursorX, cursorY - 12.f}, {cursorX, cursorY + 12.f}, 2.f);
    drawLineCursor(canvas, {cursorX, cursorY - 12.f}, {cursorX - 6.f, cursorY - 6.f}, 2.f);
    drawLineCursor(canvas, {cursorX, cursorY - 12.f}, {cursorX + 6.f, cursorY - 6.f}, 2.f);
    drawLineCursor(canvas, {cursorX, cursorY + 12.f}, {cursorX - 6.f, cursorY + 6.f}, 2.f);
    drawLineCursor(canvas, {cursorX, cursorY + 12.f}, {cursorX + 6.f, cursorY + 6.f}, 2.f);
    return;
  case flux::compositor::CursorShape::ResizeNESW:
    drawLineCursor(canvas, {cursorX - 10.f, cursorY + 10.f}, {cursorX + 10.f, cursorY - 10.f}, 2.f);
    return;
  case flux::compositor::CursorShape::ResizeNWSE:
    drawLineCursor(canvas, {cursorX - 10.f, cursorY - 10.f}, {cursorX + 10.f, cursorY + 10.f}, 2.f);
    return;
  case flux::compositor::CursorShape::ResizeAll:
    drawLineCursor(canvas, {cursorX - 12.f, cursorY}, {cursorX + 12.f, cursorY}, 2.f);
    drawLineCursor(canvas, {cursorX, cursorY - 12.f}, {cursorX, cursorY + 12.f}, 2.f);
    return;
  case flux::compositor::CursorShape::NotAllowed:
    canvas.drawCircle({cursorX + 8.f, cursorY + 8.f},
                      9.f,
                      flux::FillStyle::none(),
                      flux::StrokeStyle::solid(flux::Colors::white, 4.f));
    canvas.drawLine({cursorX + 2.f, cursorY + 14.f},
                    {cursorX + 14.f, cursorY + 2.f},
                    flux::StrokeStyle::solid(flux::Colors::white, 4.f));
    canvas.drawCircle({cursorX + 8.f, cursorY + 8.f},
                      9.f,
                      flux::FillStyle::none(),
                      flux::StrokeStyle::solid(flux::Colors::black, 1.f));
    canvas.drawLine({cursorX + 2.f, cursorY + 14.f},
                    {cursorX + 14.f, cursorY + 2.f},
                    flux::StrokeStyle::solid(flux::Colors::black, 1.f));
    return;
  case flux::compositor::CursorShape::Arrow:
    drawArrowCursor(canvas, cursorX, cursorY);
    return;
  }
}

} // namespace

int main(int, char**) {
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);

  try {
    auto device = flux::platform::KmsDevice::open();
    auto outputs = device->outputs();
    if (outputs.empty()) {
      std::fprintf(stderr, "flux-compositor: no connected KMS outputs\n");
      return 1;
    }

    flux::platform::KmsOutput const& output = outputs.front();
    flux::compositor::WaylandServer wayland({
        .name = output.name(),
        .width = static_cast<std::int32_t>(output.width()),
        .height = static_cast<std::int32_t>(output.height()),
        .refreshMilliHz = static_cast<std::int32_t>(output.refreshRateMilliHz()),
    });
    device->setInputHandler([&wayland](flux::platform::KmsInputEvent const& event) {
      if (debugCompositorInput()) {
        std::fprintf(stderr,
                     "flux-compositor: input kind=%u dx=%.2f dy=%.2f x=%.1f y=%.1f button=%u pressed=%d key=%u\n",
                     static_cast<unsigned int>(event.kind),
                     event.dx,
                     event.dy,
                     event.x,
                     event.y,
                     event.button,
                     event.pressed,
                     event.key);
      }
      switch (event.kind) {
      case flux::platform::KmsInputEvent::Kind::PointerMotion:
        wayland.handlePointerMotion(event.dx, event.dy, event.timeMs);
        break;
      case flux::platform::KmsInputEvent::Kind::PointerPosition:
        wayland.handlePointerPosition(event.x, event.y, event.timeMs);
        break;
      case flux::platform::KmsInputEvent::Kind::PointerButton:
        wayland.handlePointerButton(event.button, event.pressed, event.timeMs);
        break;
      case flux::platform::KmsInputEvent::Kind::PointerAxis:
        wayland.handlePointerAxis(event.dx, event.dy, event.timeMs);
        break;
      case flux::platform::KmsInputEvent::Kind::Key:
        wayland.handleKeyboardKey(event.key, event.pressed, event.timeMs);
        break;
      }
    });

    flux::configureVulkanCanvasRuntime(device->requiredVulkanInstanceExtensions(), device->cacheDir());
    auto& vulkan = flux::VulkanContext::instance();
    vulkan.addRequiredDeviceExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
    vulkan.addRequiredDeviceExtension(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
    vulkan.addRequiredDeviceExtension(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    VkInstance instance = flux::ensureSharedVulkanInstance();
    VkSurfaceKHR surface = output.createVulkanSurface(instance);

    static flux::FreeTypeTextSystem textSystem;
    std::unique_ptr<flux::Canvas> canvas = flux::createVulkanCanvas(surface, 1u, textSystem);
    canvas->updateDpiScale(1.f, 1.f);
    canvas->resize(static_cast<int>(output.width()), static_cast<int>(output.height()));

    std::fprintf(stderr,
                 "flux-compositor: presenting %ux%u on %s\n",
                 output.width(),
                 output.height(),
                 output.name().c_str());

    flux::Color const clearColor{0.20f, 0.50f, 0.95f, 1.0f};
    std::unordered_map<std::uint64_t, CachedClientImage> clientImages;
    std::unordered_map<std::uint64_t, SurfaceVisualState> surfaceVisuals;
    CachedClientImage cursorImage;
    while (gRunning.load(std::memory_order_relaxed) && !device->shouldTerminate()) {
      device->pollEvents(0);
      wayland.dispatch();
      if (!device->isVtForeground()) {
        device->pollEvents(250);
        wayland.dispatch();
        continue;
      }

      output.waitForVblank();
      device->pollEvents(0);
      wayland.dispatch();
      if (!device->isVtForeground()) continue;

      canvas->beginFrame();
      canvas->clear(clearColor);
      auto committedSurfaces = wayland.committedSurfaces();
      auto const frameTime = std::chrono::steady_clock::now();
      std::unordered_set<std::uint64_t> liveSurfaceIds;
      liveSurfaceIds.reserve(committedSurfaces.size());
      for (auto const& clientSurface : committedSurfaces) {
        liveSurfaceIds.insert(clientSurface.id);
        auto& visual = surfaceVisuals[clientSurface.id];
        if (visual.firstSeen.time_since_epoch().count() == 0) visual.firstSeen = frameTime;
        auto& cached = clientImages[clientSurface.id];
        updateCachedImage(wayland, *canvas, clientSurface, cached);
        if (!cached.image) continue;

        float const windowX = static_cast<float>(clientSurface.x);
        float const windowY = static_cast<float>(clientSurface.y);
        float const windowWidth = static_cast<float>(clientSurface.width);
        float const windowHeight = static_cast<float>(clientSurface.height);
        float const titleBarHeight = static_cast<float>(clientSurface.titleBarHeight);
        float const animationMs = static_cast<float>(
            std::chrono::duration_cast<std::chrono::milliseconds>(frameTime - visual.firstSeen).count());
        float const openProgress = easeOutCubic(animationMs / 140.f);
        float const openScale = 0.965f + 0.035f * openProgress;
        float const openOpacity = openProgress;
        float const outerHeight = windowHeight + titleBarHeight;
        flux::Point const pivot{windowX + windowWidth * 0.5f, windowY - titleBarHeight + outerHeight * 0.5f};
        canvas->save();
        canvas->setOpacity(canvas->opacity() * openOpacity);
        if (openScale < 1.f) {
          canvas->translate(pivot.x, pivot.y);
          canvas->scale(openScale);
          canvas->translate(-pivot.x, -pivot.y);
        }
        if (titleBarHeight > 0.f) {
          flux::Color const titleFill =
              clientSurface.focused ? flux::Color{0.10f, 0.12f, 0.14f, 1.f}
                                    : flux::Color{0.30f, 0.32f, 0.36f, 1.f};
          flux::Color const borderColor =
              clientSurface.focused ? flux::Color{0.02f, 0.03f, 0.04f, 1.f}
                                    : flux::Color{0.18f, 0.19f, 0.21f, 1.f};
          canvas->drawRect(flux::Rect::sharp(windowX, windowY - titleBarHeight, windowWidth, titleBarHeight),
                           flux::CornerRadius{0.f},
                           flux::FillStyle::solid(titleFill),
                           flux::StrokeStyle::none(),
                           flux::ShadowStyle::none());
          canvas->drawRect(flux::Rect::sharp(windowX - 1.f,
                                             windowY - titleBarHeight - 1.f,
                                             windowWidth + 2.f,
                                             windowHeight + titleBarHeight + 2.f),
                           flux::CornerRadius{0.f},
                           flux::FillStyle::none(),
                           flux::StrokeStyle::solid(borderColor, 1.f),
                           flux::ShadowStyle::none());
          float constexpr closeSize = 18.f;
          float constexpr closeInset = 5.f;
          float const closeX = windowX + windowWidth - closeInset - closeSize;
          float const closeY = windowY - titleBarHeight + closeInset;
          flux::Color const closeFill =
              clientSurface.focused ? flux::Color{0.86f, 0.20f, 0.22f, 1.f}
                                    : flux::Color{0.48f, 0.20f, 0.22f, 1.f};
          flux::Color const closeStroke =
              clientSurface.focused ? flux::Color{0.98f, 0.88f, 0.88f, 1.f}
                                    : flux::Color{0.68f, 0.58f, 0.58f, 1.f};
          canvas->drawCircle({closeX + closeSize * 0.5f, closeY + closeSize * 0.5f},
                             closeSize * 0.5f,
                             flux::FillStyle::solid(closeFill),
                             flux::StrokeStyle::none());
          canvas->drawLine({closeX + 6.f, closeY + 6.f},
                           {closeX + closeSize - 6.f, closeY + closeSize - 6.f},
                           flux::StrokeStyle::solid(closeStroke, 1.5f));
          canvas->drawLine({closeX + closeSize - 6.f, closeY + 6.f},
                           {closeX + 6.f, closeY + closeSize - 6.f},
                           flux::StrokeStyle::solid(closeStroke, 1.5f));
          float const titleLeft = windowX + 10.f;
          float const titleWidth = std::max(0.f, closeX - titleLeft - 8.f);
          if (titleWidth > 0.f && !clientSurface.title.empty()) {
            flux::Font titleFont{};
            titleFont.size = 13.f;
            titleFont.weight = 500.f;
            flux::TextLayoutOptions titleOptions{
                .verticalAlignment = flux::VerticalAlignment::Center,
                .wrapping = flux::TextWrapping::NoWrap,
                .maxLines = 1,
            };
            flux::Color const titleColor =
                clientSurface.focused ? flux::Color{0.94f, 0.96f, 0.98f, 1.f}
                                      : flux::Color{0.72f, 0.75f, 0.80f, 1.f};
            flux::TextSystem& compositorTextSystem = textSystem;
            auto titleLayout =
                compositorTextSystem.layout(clientSurface.title,
                                            titleFont,
                                            titleColor,
                                            flux::Rect::sharp(titleLeft,
                                                              windowY - titleBarHeight,
                                                              titleWidth,
                                                              titleBarHeight),
                                            titleOptions);
            if (titleLayout) {
              canvas->save();
              canvas->clipRect(flux::Rect::sharp(titleLeft,
                                                 windowY - titleBarHeight,
                                                 titleWidth,
                                                 titleBarHeight));
              canvas->drawTextLayout(*titleLayout, {0.f, 0.f});
              canvas->restore();
            }
          }
          flux::Color const gripColor =
              clientSurface.focused ? flux::Color{0.78f, 0.82f, 0.88f, 1.f}
                                    : flux::Color{0.55f, 0.58f, 0.64f, 1.f};
          float constexpr grip = 10.f;
          float constexpr gripStroke = 2.f;
          auto drawGrip = [&](float x, float y) {
            canvas->drawRect(flux::Rect::sharp(x, y, grip, gripStroke),
                             flux::CornerRadius{0.f},
                             flux::FillStyle::solid(gripColor),
                             flux::StrokeStyle::none(),
                             flux::ShadowStyle::none());
            canvas->drawRect(flux::Rect::sharp(x, y, gripStroke, grip),
                             flux::CornerRadius{0.f},
                             flux::FillStyle::solid(gripColor),
                             flux::StrokeStyle::none(),
                             flux::ShadowStyle::none());
          };
          drawGrip(windowX + 3.f, windowY + 3.f);
          drawGrip(windowX + windowWidth - grip - 3.f, windowY + 3.f);
          drawGrip(windowX + 3.f, windowY + windowHeight - grip - 3.f);
          drawGrip(windowX + windowWidth - grip - 3.f, windowY + windowHeight - grip - 3.f);
        }
        float const sourceWidth = clientSurface.sourceWidth > 0.f
                                      ? clientSurface.sourceWidth
                                      : static_cast<float>(cached.image->size().width);
        float const sourceHeight = clientSurface.sourceHeight > 0.f
                                       ? clientSurface.sourceHeight
                                       : static_cast<float>(cached.image->size().height);
        canvas->drawImage(*cached.image,
                          flux::Rect::sharp(clientSurface.sourceX,
                                            clientSurface.sourceY,
                                            sourceWidth,
                                            sourceHeight),
                          flux::Rect::sharp(windowX,
                                            windowY,
                                            windowWidth,
                                            windowHeight));
        canvas->restore();
      }
      if (auto snapPreview = wayland.snapPreview()) {
        flux::Rect const previewRect = flux::Rect::sharp(static_cast<float>(snapPreview->x),
                                                         static_cast<float>(snapPreview->y),
                                                         static_cast<float>(snapPreview->width),
                                                         static_cast<float>(snapPreview->height));
        canvas->drawRect(previewRect,
                         flux::CornerRadius{0.f},
                         flux::FillStyle::solid(flux::Color{0.86f, 0.93f, 1.0f, 0.22f}),
                         flux::StrokeStyle::solid(flux::Color{0.92f, 0.97f, 1.0f, 0.82f}, 2.f),
                         flux::ShadowStyle::none());
      }
      if (auto cursorSurface = wayland.cursorSurface()) {
        updateCachedImage(wayland, *canvas, *cursorSurface, cursorImage);
        if (cursorImage.image) {
          float const cursorSourceWidth = cursorSurface->sourceWidth > 0.f
                                              ? cursorSurface->sourceWidth
                                              : static_cast<float>(cursorImage.image->size().width);
          float const cursorSourceHeight = cursorSurface->sourceHeight > 0.f
                                               ? cursorSurface->sourceHeight
                                               : static_cast<float>(cursorImage.image->size().height);
          canvas->drawImage(*cursorImage.image,
                            flux::Rect::sharp(cursorSurface->sourceX,
                                              cursorSurface->sourceY,
                                              cursorSourceWidth,
                                              cursorSourceHeight),
                            flux::Rect::sharp(static_cast<float>(cursorSurface->x),
                                              static_cast<float>(cursorSurface->y),
                                              static_cast<float>(cursorSurface->width),
                                              static_cast<float>(cursorSurface->height)));
        }
      } else {
        cursorImage = {};
        float const cursorX = wayland.pointerX();
        float const cursorY = wayland.pointerY();
        drawFallbackCursor(*canvas, wayland.cursorShape(), cursorX, cursorY);
      }
      for (auto it = clientImages.begin(); it != clientImages.end();) {
        if (liveSurfaceIds.contains(it->first)) {
          ++it;
        } else {
          it = clientImages.erase(it);
        }
      }
      for (auto it = surfaceVisuals.begin(); it != surfaceVisuals.end();) {
        if (liveSurfaceIds.contains(it->first)) {
          ++it;
        } else {
          it = surfaceVisuals.erase(it);
        }
      }
      canvas->present();
      wayland.sendFrameCallbacks(monotonicMilliseconds());
    }

    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "flux-compositor: %s\n", e.what());
    return 1;
  }
}
