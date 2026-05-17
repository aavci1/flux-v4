#include "viewporter-client-protocol.h"
#include "xdg-shell-client-protocol.h"

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

constexpr int kBufferWidth = 420;
constexpr int kBufferHeight = 260;
constexpr int kStride = kBufferWidth * 4;
constexpr int kBufferSize = kStride * kBufferHeight;
constexpr int kSourceX = 60;
constexpr int kSourceY = 40;
constexpr int kSourceWidth = 240;
constexpr int kSourceHeight = 150;
constexpr int kDestinationWidth = 520;
constexpr int kDestinationHeight = 320;

std::atomic<bool> gRunning{true};

struct DemoClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  wp_viewporter* viewporter = nullptr;
  wp_viewport* viewport = nullptr;
  wl_surface* surface = nullptr;
  wl_buffer* buffer = nullptr;
  xdg_wm_base* wmBase = nullptr;
  xdg_surface* xdgSurface = nullptr;
  xdg_toplevel* toplevel = nullptr;
  void* pixels = nullptr;
  int fd = -1;
  bool configured = false;
};

int createSharedMemoryFile(std::size_t size) {
  int fd = memfd_create("flux-compositor-viewport-demo", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) throw std::runtime_error(std::string("memfd_create failed: ") + std::strerror(errno));
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    close(fd);
    throw std::runtime_error(std::string("ftruncate failed: ") + std::strerror(errno));
  }
  return fd;
}

void setPixel(std::uint8_t* dst, int x, int y, std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  std::size_t const offset = static_cast<std::size_t>(y) * kStride + static_cast<std::size_t>(x) * 4u;
  dst[offset + 0u] = blue;
  dst[offset + 1u] = green;
  dst[offset + 2u] = red;
  dst[offset + 3u] = 0xff;
}

void fillPattern(void* pixels) {
  auto* dst = static_cast<std::uint8_t*>(pixels);
  for (int y = 0; y < kBufferHeight; ++y) {
    for (int x = 0; x < kBufferWidth; ++x) {
      std::uint8_t const red = static_cast<std::uint8_t>((x * 255) / std::max(1, kBufferWidth - 1));
      std::uint8_t const green = static_cast<std::uint8_t>((y * 255) / std::max(1, kBufferHeight - 1));
      std::uint8_t const blue = static_cast<std::uint8_t>(((x / 16 + y / 16) % 2) ? 42 : 220);
      setPixel(dst, x, y, red, green, blue);
    }
  }
}

void wmBasePing(void*, xdg_wm_base* wmBase, std::uint32_t serial) {
  xdg_wm_base_pong(wmBase, serial);
}

xdg_wm_base_listener const kWmBaseListener{wmBasePing};

void xdgSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto* client = static_cast<DemoClient*>(data);
  xdg_surface_ack_configure(surface, serial);
  client->configured = true;
}

xdg_surface_listener const kXdgSurfaceListener{xdgSurfaceConfigure};

void toplevelConfigure(void*, xdg_toplevel*, std::int32_t, std::int32_t, wl_array*) {}

void toplevelClose(void*, xdg_toplevel*) {
  gRunning.store(false, std::memory_order_relaxed);
}

void toplevelConfigureBounds(void*, xdg_toplevel*, std::int32_t, std::int32_t) {}

void toplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}

xdg_toplevel_listener const kToplevelListener{
    .configure = toplevelConfigure,
    .close = toplevelClose,
    .configure_bounds = toplevelConfigureBounds,
    .wm_capabilities = toplevelWmCapabilities,
};

void registryGlobal(void* data, wl_registry* registry, std::uint32_t name, char const* interface,
                    std::uint32_t version) {
  auto* client = static_cast<DemoClient*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    client->compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
  } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
    client->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface, wp_viewporter_interface.name) == 0) {
    client->viewporter = static_cast<wp_viewporter*>(
        wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    client->wmBase = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    xdg_wm_base_add_listener(client->wmBase, &kWmBaseListener, client);
  }
}

void registryRemove(void*, wl_registry*, std::uint32_t) {}

wl_registry_listener const kRegistryListener{registryGlobal, registryRemove};

void createBuffer(DemoClient& client) {
  client.fd = createSharedMemoryFile(kBufferSize);
  client.pixels = mmap(nullptr, kBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, client.fd, 0);
  if (client.pixels == MAP_FAILED) {
    client.pixels = nullptr;
    throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
  }
  fillPattern(client.pixels);

  wl_shm_pool* pool = wl_shm_create_pool(client.shm, client.fd, kBufferSize);
  client.buffer =
      wl_shm_pool_create_buffer(pool, 0, kBufferWidth, kBufferHeight, kStride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
}

void destroyClient(DemoClient& client) {
  if (client.viewport) wp_viewport_destroy(client.viewport);
  if (client.buffer) wl_buffer_destroy(client.buffer);
  if (client.toplevel) xdg_toplevel_destroy(client.toplevel);
  if (client.xdgSurface) xdg_surface_destroy(client.xdgSurface);
  if (client.surface) wl_surface_destroy(client.surface);
  if (client.viewporter) wp_viewporter_destroy(client.viewporter);
  if (client.wmBase) xdg_wm_base_destroy(client.wmBase);
  if (client.shm) wl_shm_destroy(client.shm);
  if (client.compositor) wl_compositor_destroy(client.compositor);
  if (client.registry) wl_registry_destroy(client.registry);
  if (client.pixels) munmap(client.pixels, kBufferSize);
  if (client.fd >= 0) close(client.fd);
  if (client.display) wl_display_disconnect(client.display);
}

std::string displayError(DemoClient const& client) {
  if (!client.display) return "display is not connected";
  int const error = wl_display_get_error(client.display);
  if (error == EPROTO) {
    wl_interface const* interface = nullptr;
    std::uint32_t id = 0;
    std::uint32_t const code = wl_display_get_protocol_error(client.display, &interface, &id);
    std::string message = "Wayland protocol error";
    if (interface) {
      message += " on ";
      message += interface->name;
    }
    message += " object ";
    message += std::to_string(id);
    message += " code ";
    message += std::to_string(code);
    return message;
  }
  return std::strerror(error);
}

} // namespace

int main() {
  DemoClient client;
  try {
    client.display = wl_display_connect(nullptr);
    if (!client.display) throw std::runtime_error("wl_display_connect failed");

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    wl_display_roundtrip(client.display);
    if (!client.compositor || !client.shm || !client.wmBase || !client.viewporter) {
      throw std::runtime_error("compositor is missing wl_compositor, wl_shm, xdg_wm_base, or wp_viewporter");
    }

    client.surface = wl_compositor_create_surface(client.compositor);
    client.xdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, client.surface);
    xdg_surface_add_listener(client.xdgSurface, &kXdgSurfaceListener, &client);
    client.toplevel = xdg_surface_get_toplevel(client.xdgSurface);
    xdg_toplevel_add_listener(client.toplevel, &kToplevelListener, &client);
    xdg_toplevel_set_title(client.toplevel, "Flux Viewporter demo");
    xdg_toplevel_set_app_id(client.toplevel, "flux-compositor-viewport-demo");
    wl_surface_commit(client.surface);

    while (!client.configured) {
      if (wl_display_dispatch(client.display) < 0) {
        throw std::runtime_error("initial configure failed: " + displayError(client));
      }
    }

    createBuffer(client);
    client.viewport = wp_viewporter_get_viewport(client.viewporter, client.surface);
    wp_viewport_set_source(client.viewport,
                           wl_fixed_from_int(kSourceX),
                           wl_fixed_from_int(kSourceY),
                           wl_fixed_from_int(kSourceWidth),
                           wl_fixed_from_int(kSourceHeight));
    wp_viewport_set_destination(client.viewport, kDestinationWidth, kDestinationHeight);
    wl_surface_attach(client.surface, client.buffer, 0, 0);
    wl_surface_damage_buffer(client.surface, 0, 0, kBufferWidth, kBufferHeight);
    wl_surface_commit(client.surface);
    wl_display_flush(client.display);
    std::fprintf(stderr,
                 "flux-compositor-viewport-demo: committed %dx%d buffer scaled to %dx%d\n",
                 kSourceWidth,
                 kSourceHeight,
                 kDestinationWidth,
                 kDestinationHeight);

    while (gRunning.load(std::memory_order_relaxed)) {
      if (wl_display_dispatch(client.display) < 0) break;
    }

    destroyClient(client);
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "flux-compositor-viewport-demo: %s\n", e.what());
    destroyClient(client);
    return 1;
  }
}
