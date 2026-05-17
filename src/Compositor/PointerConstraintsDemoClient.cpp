#include "DemoClientSupport.hpp"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
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

constexpr int kWidth = 440;
constexpr int kHeight = 240;
constexpr int kStride = kWidth * 4;
constexpr int kBufferSize = kStride * kHeight;

std::atomic<bool> gRunning{true};

struct DemoClient {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  wl_seat* seat = nullptr;
  wl_pointer* pointer = nullptr;
  zwp_relative_pointer_manager_v1* relativePointerManager = nullptr;
  zwp_relative_pointer_v1* relativePointer = nullptr;
  zwp_pointer_constraints_v1* pointerConstraints = nullptr;
  zwp_locked_pointer_v1* lockedPointer = nullptr;
  wl_surface* surface = nullptr;
  wl_buffer* buffer = nullptr;
  xdg_wm_base* wmBase = nullptr;
  xdg_surface* xdgSurface = nullptr;
  xdg_toplevel* toplevel = nullptr;
  void* pixels = nullptr;
  int fd = -1;
  int motionCount = 0;
  bool configured = false;
  bool locked = false;
};

int createSharedMemoryFile(std::size_t size) {
  int fd = memfd_create("flux-compositor-pointer-constraints-demo", MFD_CLOEXEC | MFD_ALLOW_SEALING);
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
      bool const target = x > kWidth / 2 - 30 && x < kWidth / 2 + 30 && y > kHeight / 2 - 30 && y < kHeight / 2 + 30;
      bool const stripe = ((x / 18) + (y / 18)) % 2 == 0;
      std::uint8_t const red = target ? 250 : (stripe ? 42 : 18);
      std::uint8_t const green = target ? 70 : (stripe ? 190 : 90);
      std::uint8_t const blue = target ? 70 : (stripe ? 120 : 210);
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
void toplevelClose(void*, xdg_toplevel*) { gRunning.store(false, std::memory_order_relaxed); }
void toplevelConfigureBounds(void*, xdg_toplevel*, std::int32_t, std::int32_t) {}
void toplevelWmCapabilities(void*, xdg_toplevel*, wl_array*) {}

xdg_toplevel_listener const kToplevelListener{
    .configure = toplevelConfigure,
    .close = toplevelClose,
    .configure_bounds = toplevelConfigureBounds,
    .wm_capabilities = toplevelWmCapabilities,
};

void pointerEnter(void*, wl_pointer*, std::uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {
  std::fprintf(stderr, "flux-compositor-pointer-constraints-demo: pointer entered window\n");
}

void pointerLeave(void*, wl_pointer*, std::uint32_t, wl_surface*) {
  std::fprintf(stderr, "flux-compositor-pointer-constraints-demo: pointer left window\n");
}

void pointerMotion(void*, wl_pointer*, std::uint32_t, wl_fixed_t, wl_fixed_t) {}
void pointerButton(void*, wl_pointer*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}
void pointerAxis(void*, wl_pointer*, std::uint32_t, std::uint32_t, wl_fixed_t) {}
void pointerFrame(void*, wl_pointer*) {}
void pointerAxisSource(void*, wl_pointer*, std::uint32_t) {}
void pointerAxisStop(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
void pointerAxisDiscrete(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
void pointerAxisValue120(void*, wl_pointer*, std::uint32_t, std::int32_t) {}
void pointerAxisRelativeDirection(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}

wl_pointer_listener const kPointerListener{
    .enter = pointerEnter,
    .leave = pointerLeave,
    .motion = pointerMotion,
    .button = pointerButton,
    .axis = pointerAxis,
    .frame = pointerFrame,
    .axis_source = pointerAxisSource,
    .axis_stop = pointerAxisStop,
    .axis_discrete = pointerAxisDiscrete,
    .axis_value120 = pointerAxisValue120,
    .axis_relative_direction = pointerAxisRelativeDirection,
};

void relativeMotion(void* data,
                    zwp_relative_pointer_v1*,
                    std::uint32_t,
                    std::uint32_t,
                    wl_fixed_t dx,
                    wl_fixed_t dy,
                    wl_fixed_t,
                    wl_fixed_t) {
  auto* client = static_cast<DemoClient*>(data);
  ++client->motionCount;
  std::fprintf(stderr,
               "flux-compositor-pointer-constraints-demo: relative #%d dx=%.2f dy=%.2f\n",
               client->motionCount,
               wl_fixed_to_double(dx),
               wl_fixed_to_double(dy));
}

zwp_relative_pointer_v1_listener const kRelativePointerListener{
    .relative_motion = relativeMotion,
};

void locked(void* data, zwp_locked_pointer_v1*) {
  auto* client = static_cast<DemoClient*>(data);
  client->locked = true;
  std::fprintf(stderr, "flux-compositor-pointer-constraints-demo: locked\n");
}

void unlocked(void* data, zwp_locked_pointer_v1*) {
  auto* client = static_cast<DemoClient*>(data);
  client->locked = false;
  std::fprintf(stderr, "flux-compositor-pointer-constraints-demo: unlocked\n");
}

zwp_locked_pointer_v1_listener const kLockedPointerListener{
    .locked = locked,
    .unlocked = unlocked,
};

void registryGlobal(void* data, wl_registry* registry, std::uint32_t name, char const* interface,
                    std::uint32_t version) {
  auto* client = static_cast<DemoClient*>(data);
  if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    client->compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4u)));
  } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
    client->shm = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    client->seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 7u)));
  } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
    client->wmBase = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    xdg_wm_base_add_listener(client->wmBase, &kWmBaseListener, client);
  } else if (std::strcmp(interface, zwp_relative_pointer_manager_v1_interface.name) == 0) {
    client->relativePointerManager = static_cast<zwp_relative_pointer_manager_v1*>(
        wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1));
  } else if (std::strcmp(interface, zwp_pointer_constraints_v1_interface.name) == 0) {
    client->pointerConstraints =
        static_cast<zwp_pointer_constraints_v1*>(wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1));
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
  if (client.lockedPointer) zwp_locked_pointer_v1_destroy(client.lockedPointer);
  if (client.pointerConstraints) zwp_pointer_constraints_v1_destroy(client.pointerConstraints);
  if (client.relativePointer) zwp_relative_pointer_v1_destroy(client.relativePointer);
  if (client.relativePointerManager) zwp_relative_pointer_manager_v1_destroy(client.relativePointerManager);
  if (client.pointer) wl_pointer_destroy(client.pointer);
  if (client.buffer) wl_buffer_destroy(client.buffer);
  if (client.toplevel) xdg_toplevel_destroy(client.toplevel);
  if (client.xdgSurface) xdg_surface_destroy(client.xdgSurface);
  if (client.surface) wl_surface_destroy(client.surface);
  if (client.wmBase) xdg_wm_base_destroy(client.wmBase);
  if (client.seat) wl_seat_destroy(client.seat);
  if (client.shm) wl_shm_destroy(client.shm);
  if (client.compositor) wl_compositor_destroy(client.compositor);
  if (client.registry) wl_registry_destroy(client.registry);
  if (client.pixels) munmap(client.pixels, kBufferSize);
  if (client.fd >= 0) close(client.fd);
  if (client.display) wl_display_disconnect(client.display);
}

std::string displayError(DemoClient const& client) {
  if (!client.display) return "display is not connected";
  return std::strerror(wl_display_get_error(client.display));
}

} // namespace

int main() {
  DemoClient client;
  try {
    client.display = flux::compositor::demo::connectDisplay("flux-compositor-pointer-constraints-demo");
    if (!client.display) throw std::runtime_error("wl_display_connect failed");

    client.registry = wl_display_get_registry(client.display);
    wl_registry_add_listener(client.registry, &kRegistryListener, &client);
    if (!flux::compositor::demo::roundtripWithTimeout(client.display, 3000)) {
      throw std::runtime_error("registry roundtrip timed out");
    }
    if (!client.compositor || !client.shm || !client.seat || !client.wmBase ||
        !client.relativePointerManager || !client.pointerConstraints) {
      throw std::runtime_error("compositor is missing required pointer constraint globals");
    }

    client.pointer = wl_seat_get_pointer(client.seat);
    wl_pointer_add_listener(client.pointer, &kPointerListener, &client);
    client.relativePointer =
        zwp_relative_pointer_manager_v1_get_relative_pointer(client.relativePointerManager, client.pointer);
    zwp_relative_pointer_v1_add_listener(client.relativePointer, &kRelativePointerListener, &client);

    client.surface = wl_compositor_create_surface(client.compositor);
    client.xdgSurface = xdg_wm_base_get_xdg_surface(client.wmBase, client.surface);
    xdg_surface_add_listener(client.xdgSurface, &kXdgSurfaceListener, &client);
    client.toplevel = xdg_surface_get_toplevel(client.xdgSurface);
    xdg_toplevel_add_listener(client.toplevel, &kToplevelListener, &client);
    xdg_toplevel_set_title(client.toplevel, "Flux Pointer Constraints demo");
    xdg_toplevel_set_app_id(client.toplevel, "flux-compositor-pointer-constraints-demo");
    wl_surface_commit(client.surface);

    if (!flux::compositor::demo::waitUntil(client.display, [&] { return client.configured; }, 3000)) {
      throw std::runtime_error("initial configure timed out: " + displayError(client));
    }

    createBuffer(client);
    wl_surface_attach(client.surface, client.buffer, 0, 0);
    wl_surface_damage_buffer(client.surface, 0, 0, kWidth, kHeight);
    wl_surface_commit(client.surface);
    client.lockedPointer = zwp_pointer_constraints_v1_lock_pointer(client.pointerConstraints,
                                                                   client.surface,
                                                                   client.pointer,
                                                                   nullptr,
                                                                   ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    zwp_locked_pointer_v1_add_listener(client.lockedPointer, &kLockedPointerListener, &client);
    wl_display_flush(client.display);
    std::fprintf(stderr,
                 "flux-compositor-pointer-constraints-demo: move pointer over the window; close it or press Ctrl+C to exit\n");

    while (gRunning.load(std::memory_order_relaxed)) {
      if (flux::compositor::demo::dispatchWithTimeout(client.display, 250) < 0) break;
    }

    destroyClient(client);
    return 0;
  } catch (std::exception const& e) {
    std::fprintf(stderr, "flux-compositor-pointer-constraints-demo: %s\n", e.what());
    destroyClient(client);
    return 1;
  }
}
