#include <Flux/Core/Color.hpp>
#include <Flux/Core/Geometry.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Image.hpp>
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
  std::uint64_t serial = 0;
  std::shared_ptr<flux::Image> image;
  bool logged = false;
};

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
      std::unordered_set<std::uint64_t> liveSurfaceIds;
      liveSurfaceIds.reserve(committedSurfaces.size());
      for (auto const& clientSurface : committedSurfaces) {
        liveSurfaceIds.insert(clientSurface.id);
        auto& cached = clientImages[clientSurface.id];
        if (!cached.image || cached.serial != clientSurface.serial) {
          cached.serial = clientSurface.serial;
          cached.image.reset();
          cached.logged = false;
          if (!clientSurface.rgbaPixels.empty()) {
            cached.image = flux::Image::fromRgbaPixels(static_cast<std::uint32_t>(clientSurface.width),
                                                       static_cast<std::uint32_t>(clientSurface.height),
                                                       clientSurface.rgbaPixels,
                                                       canvas->gpuDevice());
          } else if (!clientSurface.dmabufPlanes.empty()) {
            std::vector<std::uint8_t> fallbackPixels;
            std::vector<int> fds = wayland.duplicateDmabufFds(clientSurface.id);
            if (fds.size() == clientSurface.dmabufPlanes.size()) {
              std::vector<flux::Image::DmabufPlane> planes;
              planes.reserve(clientSurface.dmabufPlanes.size());
              for (std::size_t i = 0; i < clientSurface.dmabufPlanes.size(); ++i) {
                planes.push_back({
                    .fd = fds[i],
                    .offset = clientSurface.dmabufPlanes[i].offset,
                    .stride = clientSurface.dmabufPlanes[i].stride,
                    .modifier = clientSurface.dmabufPlanes[i].modifier,
                });
              }
              try {
                cached.image = flux::Image::fromDmabuf({
                    .width = static_cast<std::uint32_t>(clientSurface.width),
                    .height = static_cast<std::uint32_t>(clientSurface.height),
                    .drmFormat = clientSurface.dmabufFormat,
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
            if (wayland.copyDmabufToRgba(clientSurface.id, fallbackPixels)) {
              cached.image = flux::Image::fromRgbaPixels(static_cast<std::uint32_t>(clientSurface.width),
                                                         static_cast<std::uint32_t>(clientSurface.height),
                                                         fallbackPixels,
                                                         canvas->gpuDevice());
              if (cached.image && !cached.logged) {
                std::fprintf(stderr, "flux-compositor: displaying readable DMABUF contents\n");
                cached.logged = true;
              }
            }
          }
        }
        if (!cached.image) continue;

        canvas->drawImage(*cached.image,
                          flux::Rect::sharp(0.f,
                                            0.f,
                                            static_cast<float>(clientSurface.width),
                                            static_cast<float>(clientSurface.height)),
                          flux::Rect::sharp(static_cast<float>(clientSurface.x),
                                            static_cast<float>(clientSurface.y),
                                            static_cast<float>(clientSurface.width),
                                            static_cast<float>(clientSurface.height)));
      }
      float const cursorX = wayland.pointerX();
      float const cursorY = wayland.pointerY();
      flux::Path cursor;
      cursor.moveTo({cursorX, cursorY});
      cursor.lineTo({cursorX + 2.f, cursorY + 22.f});
      cursor.lineTo({cursorX + 8.f, cursorY + 16.f});
      cursor.lineTo({cursorX + 14.f, cursorY + 30.f});
      cursor.lineTo({cursorX + 19.f, cursorY + 28.f});
      cursor.lineTo({cursorX + 13.f, cursorY + 14.f});
      cursor.lineTo({cursorX + 21.f, cursorY + 14.f});
      cursor.close();
      canvas->drawPath(cursor,
                       flux::FillStyle::solid(flux::Colors::white),
                       flux::StrokeStyle::solid(flux::Colors::black, 1.f));
      for (auto it = clientImages.begin(); it != clientImages.end();) {
        if (liveSurfaceIds.contains(it->first)) {
          ++it;
        } else {
          it = clientImages.erase(it);
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
