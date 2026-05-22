#include "DemoClientSupport.hpp"
#include "xdg-activation-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int kWidth = 320;
constexpr int kHeight = 210;
constexpr int kStride = kWidth * 4;
constexpr int kBufferSize = kStride * kHeight;

std::atomic<bool> gRunning{true};

struct Window {
  wl_surface* surface = nullptr;
  xdg_surface* xdgSurface = nullptr;
  xdg_toplevel* toplevel = nullptr;
  wl_buffer* buffer = nullptr;
  void* pixels = nullptr;
  int fd = -1;
  bool committed = false;
};

struct DemoClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  xdg_wm_base* wmBase = nullptr;
  xdg_activation_v1* activation = nullptr;
  xdg_activation_token_v1* token = nullptr;
  Window first;
  Window second;
  bool activationRequested = false;
};

int createSharedMemoryFile(std::size_t size) {
  int fd = memfd_create("lambda-window-manager-activation-demo", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) throw std::runtime_error(std::string("memfd_create failed: ") + std::strerror(errno));
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    close(fd);
    throw std::runtime_error(std::string("ftruncate failed: ") + std::strerror(errno));
  }
  return fd;
}

void fillPattern(void* pixels, bool second) {
  auto* dst = static_cast<std::uint8_t*>(pixels);
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      std::size_t const offset = static_cast<std::size_t>(y) * kStride + static_cast<std::size_t>(x) * 4u;
      bool const stripe = ((x + y) / 22) % 2 == 0;
      dst[offset + 0u] = second ? static_cast<std::uint8_t>(stripe ? 210 : 80) : static_cast<std::uint8_t>(60);
      dst[offset + 1u] = second ? static_cast<std::uint8_t>(80) : static_cast<std::uint8_t>(stripe ? 210 : 80);
      dst[offset + 2u] = static_cast<std::uint8_t>(stripe ? 70 : 230);
      dst[offset + 3u] = 0xff;
    }
  }
}

void createBuffer(DemoClient& client, Window& window, bool second) {
  window.fd = createSharedMemoryFile(kBufferSize);
  window.pixels = mmap(nullptr, kBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, window.fd, 0);
  if (window.pixels == MAP_FAILED) {
    window.pixels = nullptr;
    throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
  }
  fillPattern(window.pixels, second);
  wl_shm_pool* pool = wl_shm_create_pool(client.shm, window.fd, kBufferSize);
  window.buffer = wl_shm_pool_create_buffer(pool, 0, kWidth, kHeight, kStride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
}

void commitBuffer(Window& window) {
  wl_surface_attach(window.surface, window.buffer, 0, 0);
  wl_surface_damage_buffer(window.surface, 0, 0, kWidth, kHeight);
  wl_surface_commit(window.surface);
  window.committed = true;
}

void tokenDone(void* data, xdg_activation_token_v1* token, char const* tokenString) {
  auto* client = static_cast<DemoClient*>(data);
  std::fprintf(stderr, "lambda-window-manager-activation-demo: activating second window with token %s\n", tokenString);
  xdg_activation_v1_activate(client->activation, tokenString, client->second.surface);
  xdg_activation_token_v1_destroy(token);
  client->token = nullptr;
  wl_display_flush(client->display);
}

xdg_activation_token_v1_listener const kTokenListener{tokenDone};

void requestActivation(DemoClient& client) {
  if (client.activationRequested || !client.first.committed || !client.second.committed) return;
  client.activationRequested = true;
  client.token = xdg_activation_v1_get_activation_token(client.activation);
  xdg_activation_token_v1_add_listener(client.token, &kTokenListener, &client);
  xdg_activation_token_v1_set_app_id(client.token, "lambda-window-manager-activation-demo");
  xdg_activation_token_v1_set_surface(client.token, client.first.surface);
  xdg_activation_token_v1_commit(client.token);
  wl_display_flush(client.display);
  std::fprintf(stderr, "lambda-window-manager-activation-demo: requested activation token\n");
}

void wmBasePing(void*, xdg_wm_base* wmBase, std::uint32_t serial) {
  xdg_wm_base_pong(wmBase, serial);
}

xdg_wm_base_listener const kWmBaseListener{wmBasePing};

void xdgSurfaceConfigure(void* data, xdg_surface* surface, std::uint32_t serial) {
  auto* client = static_cast<DemoClient*>(data);
  xdg_surface_ack_configure(surface, serial);
  Window* window = surface == client->first.xdgSurface ? &client->first : &client->second;
  if (!window->buffer) createBuffer(*client, *window, window == &client->second);
  if (!window->committed) commitBuffer(*window);
}

xdg_surface_listener const kXdgSurfaceListener{xdgSurfaceConfigure};

void toplevelConfigure(void*, xdg_toplevel*, std::int32_t, std::int32_t, wl_array*) {}
void toplevelClose(void*, xdg_toplevel*) { gRunning.store(false, std::memory_order_relaxed); }
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
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    client->wmBase = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    xdg_wm_base_add_listener(client->wmBase, &kWmBaseListener, client);
  } else if (std::strcmp(interface, xdg_activation_v1_interface.name) == 0) {
    client->activation = static_cast<xdg_activation_v1*>(
        wl_registry_bind(registry, name, &xdg_activation_v1_interface, std::min(version, 1u)));
  }
}

void registryRemove(void*, wl_registry*, std::uint32_t) {}

wl_registry_listener const kRegistryListener{registryGlobal, registryRemove};

void createWindow(DemoClient& client, Window& window, char const* title) {
  window.surface = wl_compositor_create_surface(client.compositor);
  window.xdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, window.surface);
  xdg_surface_add_listener(window.xdgSurface, &kXdgSurfaceListener, &client);
  window.toplevel = xdg_surface_get_toplevel(window.xdgSurface);
  xdg_toplevel_add_listener(window.toplevel, &kToplevelListener, &client);
  xdg_toplevel_set_title(window.toplevel, title);
  xdg_toplevel_set_app_id(window.toplevel, "lambda-window-manager-activation-demo");
  wl_surface_commit(window.surface);
}

void destroyWindow(Window& window) {
  if (window.buffer) wl_buffer_destroy(window.buffer);
  if (window.toplevel) xdg_toplevel_destroy(window.toplevel);
  if (window.xdgSurface) xdg_surface_destroy(window.xdgSurface);
  if (window.surface) wl_surface_destroy(window.surface);
  if (window.pixels) munmap(window.pixels, kBufferSize);
  if (window.fd >= 0) close(window.fd);
}

void destroyClient(DemoClient& client) {
  if (client.token) xdg_activation_token_v1_destroy(client.token);
  destroyWindow(client.second);
  destroyWindow(client.first);
  if (client.activation) xdg_activation_v1_destroy(client.activation);
  if (client.wmBase) xdg_wm_base_destroy(client.wmBase);
  if (client.shm) wl_shm_destroy(client.shm);
  if (client.compositor) wl_compositor_destroy(client.compositor);
  if (client.registry) wl_registry_destroy(client.registry);
  if (client.display) wl_display_disconnect(client.display);
}

} // namespace

int main() {
  DemoClient client;
  try {
    client.display = flux::compositor::demo::connectDisplay("lambda-window-manager-activation-demo");
    if (!client.display) throw std::runtime_error("wl_display_connect failed");

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    if (!flux::compositor::demo::roundtripWithTimeout(client.display, 3000)) {
      throw std::runtime_error("registry roundtrip timed out");
    }
    if (!client.compositor || !client.shm || !client.wmBase || !client.activation) {
      throw std::runtime_error("compositor is missing wl_compositor, wl_shm, xdg_wm_base, or xdg_activation_v1");
    }

    createWindow(client, client.second, "Flux activation target");
    createWindow(client, client.first, "Flux activation source");

    if (!flux::compositor::demo::waitUntil(client.display,
                                           [&] { return client.first.committed && client.second.committed; },
                                           3000)) {
      throw std::runtime_error("initial window commits timed out");
    }

    std::fprintf(stderr,
                 "lambda-window-manager-activation-demo: source starts on top; target should raise in about one second\n");
    auto const activationDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < activationDeadline) {
      if (flux::compositor::demo::dispatchWithTimeout(client.display, 50) < 0) break;
    }
    requestActivation(client);
    std::fprintf(stderr,
                 "lambda-window-manager-activation-demo: expect the target window to be on top; close a window or Ctrl+C\n");
    while (gRunning.load(std::memory_order_relaxed)) {
      if (flux::compositor::demo::dispatchWithTimeout(client.display, 250) < 0) break;
    }

    destroyClient(client);
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "lambda-window-manager-activation-demo: %s\n", e.what());
    destroyClient(client);
    return 1;
  }
}
