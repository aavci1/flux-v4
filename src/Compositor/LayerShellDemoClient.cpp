#include "DemoClientSupport.hpp"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int kHeight = 48;

std::atomic<bool> gRunning{true};

struct DemoClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  zwlr_layer_shell_v1* layerShell = nullptr;
  wl_surface* surface = nullptr;
  zwlr_layer_surface_v1* layerSurface = nullptr;
  wl_buffer* buffer = nullptr;
  void* pixels = nullptr;
  int fd = -1;
  int width = 0;
  int height = kHeight;
  bool configured = false;
};

int createSharedMemoryFile(std::size_t size) {
  int fd = memfd_create("flux-compositor-layer-shell-demo", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) throw std::runtime_error(std::string("memfd_create failed: ") + std::strerror(errno));
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    close(fd);
    throw std::runtime_error(std::string("ftruncate failed: ") + std::strerror(errno));
  }
  return fd;
}

void fillBar(void* pixels, int width, int height) {
  auto* dst = static_cast<std::uint8_t*>(pixels);
  int const stride = width * 4;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      bool const accent = x < 220 || ((x / 32 + y / 12) % 2 == 0);
      std::uint8_t const red = accent ? 255 : 20;
      std::uint8_t const green = accent ? 40 : 220;
      std::uint8_t const blue = accent ? 40 : 255;
      std::size_t const offset = static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4u;
      dst[offset + 0u] = blue;
      dst[offset + 1u] = green;
      dst[offset + 2u] = red;
      dst[offset + 3u] = 0xff;
    }
  }
}

void createBuffer(DemoClient& client) {
  std::size_t const stride = static_cast<std::size_t>(client.width) * 4u;
  std::size_t const size = stride * static_cast<std::size_t>(client.height);
  client.fd = createSharedMemoryFile(size);
  client.pixels = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, client.fd, 0);
  if (client.pixels == MAP_FAILED) {
    client.pixels = nullptr;
    throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
  }
  fillBar(client.pixels, client.width, client.height);
  wl_shm_pool* pool = wl_shm_create_pool(client.shm, client.fd, static_cast<std::int32_t>(size));
  client.buffer = wl_shm_pool_create_buffer(pool,
                                            0,
                                            client.width,
                                            client.height,
                                            static_cast<std::int32_t>(stride),
                                            WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
}

void layerSurfaceConfigure(void* data, zwlr_layer_surface_v1* layerSurface, std::uint32_t serial,
                           std::uint32_t width, std::uint32_t height) {
  auto* client = static_cast<DemoClient*>(data);
  zwlr_layer_surface_v1_ack_configure(layerSurface, serial);
  client->width = static_cast<int>(std::max(1u, width));
  client->height = static_cast<int>(std::max(1u, height));
  client->configured = true;
}

void layerSurfaceClosed(void*, zwlr_layer_surface_v1*) {
  gRunning.store(false, std::memory_order_relaxed);
}

zwlr_layer_surface_v1_listener const kLayerSurfaceListener{
    .configure = layerSurfaceConfigure,
    .closed = layerSurfaceClosed,
};

void registryGlobal(void* data, wl_registry* registry, std::uint32_t name, char const* interface,
                    std::uint32_t version) {
  auto* client = static_cast<DemoClient*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    client->compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
  } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
    client->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    client->layerShell = static_cast<zwlr_layer_shell_v1*>(
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1));
  }
}

void registryRemove(void*, wl_registry*, std::uint32_t) {}
wl_registry_listener const kRegistryListener{registryGlobal, registryRemove};

std::string displayError(DemoClient const& client) {
  if (!client.display) return "display is not connected";
  int const error = wl_display_get_error(client.display);
  return std::strerror(error);
}

} // namespace

int main() {
  DemoClient client;
  try {
    client.display = flux::compositor::demo::connectDisplay("flux-compositor-layer-shell-demo");
    if (!client.display) throw std::runtime_error("wl_display_connect failed");

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    if (!flux::compositor::demo::roundtripWithTimeout(client.display, 3000)) {
      throw std::runtime_error("registry roundtrip timed out");
    }
    if (!client.compositor || !client.shm || !client.layerShell) {
      throw std::runtime_error("compositor is missing wl_compositor, wl_shm, or zwlr_layer_shell_v1");
    }

    client.surface = wl_compositor_create_surface(client.compositor);
    client.layerSurface = zwlr_layer_shell_v1_get_layer_surface(client.layerShell,
                                                                client.surface,
                                                                nullptr,
                                                                ZWLR_LAYER_SHELL_V1_LAYER_TOP,
                                                                "flux-layer-shell-demo");
    zwlr_layer_surface_v1_add_listener(client.layerSurface, &kLayerSurfaceListener, &client);
    zwlr_layer_surface_v1_set_size(client.layerSurface, 0, kHeight);
    zwlr_layer_surface_v1_set_anchor(client.layerSurface,
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                         ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    wl_surface_commit(client.surface);

    if (!flux::compositor::demo::waitUntil(client.display, [&] { return client.configured; }, 3000)) {
      throw std::runtime_error("initial configure timed out: " + displayError(client));
    }

    createBuffer(client);
    wl_surface_attach(client.surface, client.buffer, 0, 0);
    wl_surface_damage_buffer(client.surface, 0, 0, client.width, client.height);
    wl_surface_commit(client.surface);
    wl_display_flush(client.display);
    std::fprintf(stderr, "flux-compositor-layer-shell-demo: committed top layer %dx%d\n", client.width, client.height);

    while (gRunning.load(std::memory_order_relaxed)) {
      if (flux::compositor::demo::dispatchWithTimeout(client.display, 250) < 0) break;
    }
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "flux-compositor-layer-shell-demo: %s\n", e.what());
    return 1;
  }
}
