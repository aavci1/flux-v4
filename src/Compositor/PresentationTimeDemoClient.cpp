#include "DemoClientSupport.hpp"
#include "presentation-time-client-protocol.h"
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

constexpr int kWidth = 360;
constexpr int kHeight = 180;
constexpr int kStride = kWidth * 4;
constexpr int kBufferSize = kStride * kHeight;

std::atomic<bool> gRunning{true};

struct DemoClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  wp_presentation* presentation = nullptr;
  struct wp_presentation_feedback* feedback = nullptr;
  wl_surface* surface = nullptr;
  wl_buffer* buffer = nullptr;
  xdg_wm_base* wmBase = nullptr;
  xdg_surface* xdgSurface = nullptr;
  xdg_toplevel* toplevel = nullptr;
  void* pixels = nullptr;
  int fd = -1;
  std::uint32_t clockId = 0;
  bool configured = false;
  bool gotClockId = false;
  bool gotFeedback = false;
};

int createSharedMemoryFile(std::size_t size) {
  int fd = memfd_create("flux-compositor-presentation-time-demo", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) throw std::runtime_error(std::string("memfd_create failed: ") + std::strerror(errno));
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    close(fd);
    throw std::runtime_error(std::string("ftruncate failed: ") + std::strerror(errno));
  }
  return fd;
}

void fillPattern(void* pixels) {
  auto* dst = static_cast<std::uint8_t*>(pixels);
  for (int y = 0; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      bool const accent = x < 96 || y < 32;
      std::uint8_t const red = accent ? 245 : static_cast<std::uint8_t>((x * 255) / std::max(1, kWidth - 1));
      std::uint8_t const green = accent ? 220 : static_cast<std::uint8_t>((y * 255) / std::max(1, kHeight - 1));
      std::uint8_t const blue = accent ? 48 : 130;
      std::size_t const offset = static_cast<std::size_t>(y) * kStride + static_cast<std::size_t>(x) * 4u;
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

void presentationClockId(void* data, wp_presentation*, std::uint32_t clockId) {
  auto* client = static_cast<DemoClient*>(data);
  client->clockId = clockId;
  client->gotClockId = true;
  std::fprintf(stderr, "flux-compositor-presentation-time-demo: clock_id=%u\n", clockId);
}

wp_presentation_listener const kPresentationListener{
    .clock_id = presentationClockId,
};

void feedbackSyncOutput(void*, struct wp_presentation_feedback*, wl_output*) {}

void feedbackPresented(void* data,
                       struct wp_presentation_feedback*,
                       std::uint32_t tvSecHi,
                       std::uint32_t tvSecLo,
                       std::uint32_t tvNsec,
                       std::uint32_t refresh,
                       std::uint32_t seqHi,
                       std::uint32_t seqLo,
                       std::uint32_t flags) {
  auto* client = static_cast<DemoClient*>(data);
  std::uint64_t const seconds = (static_cast<std::uint64_t>(tvSecHi) << 32u) | tvSecLo;
  std::uint64_t const sequence = (static_cast<std::uint64_t>(seqHi) << 32u) | seqLo;
  std::fprintf(stderr,
               "flux-compositor-presentation-time-demo: presented sec=%llu nsec=%u refresh=%u seq=%llu flags=0x%x\n",
               static_cast<unsigned long long>(seconds),
               tvNsec,
               refresh,
               static_cast<unsigned long long>(sequence),
               flags);
  client->gotFeedback = true;
  client->feedback = nullptr;
  gRunning.store(false, std::memory_order_relaxed);
}

void feedbackDiscarded(void* data, struct wp_presentation_feedback*) {
  auto* client = static_cast<DemoClient*>(data);
  std::fprintf(stderr, "flux-compositor-presentation-time-demo: feedback discarded\n");
  client->gotFeedback = true;
  client->feedback = nullptr;
  gRunning.store(false, std::memory_order_relaxed);
}

wp_presentation_feedback_listener const kFeedbackListener{
    .sync_output = feedbackSyncOutput,
    .presented = feedbackPresented,
    .discarded = feedbackDiscarded,
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
  } else if (std::strcmp(interface, wp_presentation_interface.name) == 0) {
    client->presentation = static_cast<wp_presentation*>(
        wl_registry_bind(registry, name, &wp_presentation_interface, std::min(version, 2u)));
    wp_presentation_add_listener(client->presentation, &kPresentationListener, client);
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
  client.buffer = wl_shm_pool_create_buffer(pool, 0, kWidth, kHeight, kStride, WL_SHM_FORMAT_XRGB8888);
  wl_shm_pool_destroy(pool);
}

void destroyClient(DemoClient& client) {
  if (client.feedback) wp_presentation_feedback_destroy(client.feedback);
  if (client.buffer) wl_buffer_destroy(client.buffer);
  if (client.toplevel) xdg_toplevel_destroy(client.toplevel);
  if (client.xdgSurface) xdg_surface_destroy(client.xdgSurface);
  if (client.surface) wl_surface_destroy(client.surface);
  if (client.presentation) wp_presentation_destroy(client.presentation);
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
  return std::strerror(error);
}

} // namespace

int main() {
  DemoClient client;
  try {
    client.display = flux::compositor::demo::connectDisplay("flux-compositor-presentation-time-demo");
    if (!client.display) throw std::runtime_error("wl_display_connect failed");

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    if (!flux::compositor::demo::roundtripWithTimeout(client.display, 3000)) {
      throw std::runtime_error("registry roundtrip timed out");
    }
    if (!client.compositor || !client.shm || !client.wmBase || !client.presentation) {
      throw std::runtime_error("compositor is missing wl_compositor, wl_shm, xdg_wm_base, or wp_presentation");
    }
    if (!client.gotClockId && !flux::compositor::demo::roundtripWithTimeout(client.display, 3000)) {
      throw std::runtime_error("presentation clock_id timed out");
    }

    client.surface = wl_compositor_create_surface(client.compositor);
    client.xdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, client.surface);
    xdg_surface_add_listener(client.xdgSurface, &kXdgSurfaceListener, &client);
    client.toplevel = xdg_surface_get_toplevel(client.xdgSurface);
    xdg_toplevel_add_listener(client.toplevel, &kToplevelListener, &client);
    xdg_toplevel_set_title(client.toplevel, "Flux Presentation Time demo");
    xdg_toplevel_set_app_id(client.toplevel, "flux-compositor-presentation-time-demo");
    wl_surface_commit(client.surface);

    if (!flux::compositor::demo::waitUntil(client.display, [&] { return client.configured; }, 3000)) {
      throw std::runtime_error("initial configure timed out: " + displayError(client));
    }

    createBuffer(client);
    client.feedback = wp_presentation_feedback(client.presentation, client.surface);
    wp_presentation_feedback_add_listener(client.feedback, &kFeedbackListener, &client);
    wl_surface_attach(client.surface, client.buffer, 0, 0);
    wl_surface_damage_buffer(client.surface, 0, 0, kWidth, kHeight);
    wl_surface_commit(client.surface);
    wl_display_flush(client.display);
    std::fprintf(stderr, "flux-compositor-presentation-time-demo: committed %dx%d buffer\n", kWidth, kHeight);

    while (gRunning.load(std::memory_order_relaxed)) {
      if (flux::compositor::demo::dispatchWithTimeout(client.display, 250) < 0) break;
    }
    if (!client.gotFeedback) throw std::runtime_error("presentation feedback timed out");

    destroyClient(client);
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "flux-compositor-presentation-time-demo: %s\n", e.what());
    destroyClient(client);
    return 1;
  }
}
