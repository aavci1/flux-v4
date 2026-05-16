#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <gbm.h>
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

constexpr std::uint32_t kWidth = 360;
constexpr std::uint32_t kHeight = 220;
constexpr std::uint32_t kFormat = DRM_FORMAT_XRGB8888;

std::atomic<bool> gRunning{true};

struct DemoClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  zwp_linux_dmabuf_v1* dmabuf = nullptr;
  wl_surface* surface = nullptr;
  wl_buffer* buffer = nullptr;
  xdg_wm_base* wmBase = nullptr;
  xdg_surface* xdgSurface = nullptr;
  xdg_toplevel* toplevel = nullptr;
  int renderFd = -1;
  gbm_device* gbm = nullptr;
  gbm_bo* bo = nullptr;
  bool configured = false;
};

std::string errnoMessage(char const* call) {
  return std::string(call) + " failed: " + std::strerror(errno);
}

int openRenderNode() {
  for (int index = 128; index < 144; ++index) {
    std::string path = "/dev/dri/renderD" + std::to_string(index);
    int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd >= 0) return fd;
  }
  throw std::runtime_error("could not open /dev/dri/renderD128..renderD143");
}

void fillPattern(void* pixels, std::uint32_t stride) {
  auto* dst = static_cast<std::uint8_t*>(pixels);
  for (std::uint32_t y = 0; y < kHeight; ++y) {
    for (std::uint32_t x = 0; x < kWidth; ++x) {
      std::uint8_t const red = static_cast<std::uint8_t>(255u - (x * 180u) / std::max(1u, kWidth - 1u));
      std::uint8_t const green = static_cast<std::uint8_t>(60u + (y * 160u) / std::max(1u, kHeight - 1u));
      std::uint8_t const blue = static_cast<std::uint8_t>(((x / 20u + y / 20u) % 2u) ? 245u : 30u);
      std::size_t const offset = static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4u;
      // DRM_FORMAT_XRGB8888 is little-endian, so memory order is B, G, R, X.
      dst[offset + 0u] = blue;
      dst[offset + 1u] = green;
      dst[offset + 2u] = red;
      dst[offset + 3u] = 0xff;
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
  } else if (std::strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0) {
    client->dmabuf = static_cast<zwp_linux_dmabuf_v1*>(
        wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, std::min(version, 3u)));
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    client->wmBase = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    xdg_wm_base_add_listener(client->wmBase, &kWmBaseListener, client);
  }
}

void registryRemove(void*, wl_registry*, std::uint32_t) {}

wl_registry_listener const kRegistryListener{registryGlobal, registryRemove};

void bufferRelease(void*, wl_buffer*) {}

wl_buffer_listener const kBufferListener{bufferRelease};

void createDmabufBuffer(DemoClient& client) {
  client.renderFd = openRenderNode();
  client.gbm = gbm_create_device(client.renderFd);
  if (!client.gbm) throw std::runtime_error("gbm_create_device failed");

  std::uint64_t const modifiers[] = {DRM_FORMAT_MOD_LINEAR};
  client.bo = gbm_bo_create_with_modifiers2(client.gbm, kWidth, kHeight, kFormat, modifiers, 1,
                                            GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
  if (!client.bo) {
    client.bo = gbm_bo_create(client.gbm, kWidth, kHeight, kFormat, GBM_BO_USE_LINEAR | GBM_BO_USE_RENDERING);
  }
  if (!client.bo) throw std::runtime_error("gbm_bo_create failed");
  if (gbm_bo_get_plane_count(client.bo) != 1) {
    throw std::runtime_error("demo expected a single-plane GBM buffer");
  }

  void* mapData = nullptr;
  std::uint32_t mapStride = 0;
  void* mapped = gbm_bo_map(client.bo, 0, 0, kWidth, kHeight, GBM_BO_TRANSFER_WRITE, &mapStride, &mapData);
  if (!mapped) throw std::runtime_error("gbm_bo_map failed");
  fillPattern(mapped, mapStride);
  gbm_bo_unmap(client.bo, mapData);

  int fd = gbm_bo_get_fd(client.bo);
  if (fd < 0) throw std::runtime_error(errnoMessage("gbm_bo_get_fd"));

  std::uint64_t const modifier = gbm_bo_get_modifier(client.bo);
  std::fprintf(stderr,
               "flux-compositor-dmabuf-demo: GBM buffer stride=%u modifier=0x%016llx\n",
               gbm_bo_get_stride_for_plane(client.bo, 0),
               static_cast<unsigned long long>(modifier));
  zwp_linux_buffer_params_v1* params = zwp_linux_dmabuf_v1_create_params(client.dmabuf);
  zwp_linux_buffer_params_v1_add(params, fd, 0, gbm_bo_get_offset(client.bo, 0),
                                 gbm_bo_get_stride_for_plane(client.bo, 0),
                                 static_cast<std::uint32_t>(modifier >> 32u),
                                 static_cast<std::uint32_t>(modifier & 0xffffffffu));
  close(fd);

  client.buffer = zwp_linux_buffer_params_v1_create_immed(params, kWidth, kHeight, kFormat, 0);
  zwp_linux_buffer_params_v1_destroy(params);
  if (!client.buffer) throw std::runtime_error("zwp_linux_buffer_params_v1_create_immed failed");
  wl_buffer_add_listener(client.buffer, &kBufferListener, &client);
}

void destroyClient(DemoClient& client) {
  if (client.buffer) wl_buffer_destroy(client.buffer);
  if (client.toplevel) xdg_toplevel_destroy(client.toplevel);
  if (client.xdgSurface) xdg_surface_destroy(client.xdgSurface);
  if (client.surface) wl_surface_destroy(client.surface);
  if (client.wmBase) xdg_wm_base_destroy(client.wmBase);
  if (client.dmabuf) zwp_linux_dmabuf_v1_destroy(client.dmabuf);
  if (client.compositor) wl_compositor_destroy(client.compositor);
  if (client.registry) wl_registry_destroy(client.registry);
  if (client.bo) gbm_bo_destroy(client.bo);
  if (client.gbm) gbm_device_destroy(client.gbm);
  if (client.renderFd >= 0) close(client.renderFd);
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
    if (!client.compositor || !client.dmabuf || !client.wmBase) {
      throw std::runtime_error("compositor is missing wl_compositor, zwp_linux_dmabuf_v1, or xdg_wm_base");
    }

    client.surface = wl_compositor_create_surface(client.compositor);
    client.xdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, client.surface);
    xdg_surface_add_listener(client.xdgSurface, &kXdgSurfaceListener, &client);
    client.toplevel = xdg_surface_get_toplevel(client.xdgSurface);
    xdg_toplevel_add_listener(client.toplevel, &kToplevelListener, &client);
    xdg_toplevel_set_title(client.toplevel, "Flux DMABUF demo");
    xdg_toplevel_set_app_id(client.toplevel, "flux-compositor-dmabuf-demo");
    wl_surface_commit(client.surface);

    while (!client.configured) {
      if (wl_display_dispatch(client.display) < 0) {
        throw std::runtime_error("initial configure failed: " + displayError(client));
      }
    }

    createDmabufBuffer(client);
    wl_surface_attach(client.surface, client.buffer, 0, 0);
    wl_surface_damage_buffer(client.surface, 0, 0, static_cast<std::int32_t>(kWidth),
                             static_cast<std::int32_t>(kHeight));
    wl_surface_commit(client.surface);
    wl_display_flush(client.display);
    std::fprintf(stderr, "flux-compositor-dmabuf-demo: committed %ux%u DMABUF buffer\n", kWidth, kHeight);

    while (gRunning.load(std::memory_order_relaxed)) {
      if (wl_display_dispatch(client.display) < 0) break;
    }

    destroyClient(client);
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "flux-compositor-dmabuf-demo: %s\n", e.what());
    destroyClient(client);
    return 1;
  }
}
