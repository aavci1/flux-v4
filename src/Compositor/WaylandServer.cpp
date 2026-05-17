#include "Compositor/WaylandServer.hpp"

#include "linux-dmabuf-unstable-v1-server-protocol.h"
#include "xdg-decoration-unstable-v1-server-protocol.h"
#include "xdg-shell-server-protocol.h"

#include <drm_fourcc.h>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xkbcommon/xkbcommon.h>

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <optional>
#include <stdexcept>
#include <vector>
#include <utility>

namespace flux::compositor {
namespace {

constexpr std::int32_t kTitleBarHeight = 28;
constexpr std::int32_t kResizeGripSize = 14;
constexpr std::int32_t kMinWindowWidth = 160;
constexpr std::int32_t kMinWindowHeight = 120;
constexpr std::uint32_t kInvalidModifierIndex = ~0u;
constexpr std::int32_t kCloseButtonSize = 18;
constexpr std::int32_t kCloseButtonInset = 5;

WaylandServer* serverFrom(wl_resource* resource) {
  return static_cast<WaylandServer*>(wl_resource_get_user_data(resource));
}

template <typename T>
T* dataFrom(wl_resource* resource) {
  if (!resource) return nullptr;
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

void inertDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void outputRelease(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

extern struct wl_shm_pool_interface const shmPoolImpl;
extern struct zwp_linux_buffer_params_v1_interface const linuxBufferParamsImpl;
extern struct zxdg_toplevel_decoration_v1_interface const xdgToplevelDecorationImpl;
extern struct xdg_surface_interface const xdgSurfaceImpl;
extern struct xdg_toplevel_interface const xdgToplevelImpl;
extern struct wl_pointer_interface const pointerImpl;
extern struct wl_keyboard_interface const keyboardImpl;
extern struct wl_seat_interface const seatImpl;

} // namespace

struct WaylandServer::Surface {
  WaylandServer* server = nullptr;
  wl_resource* resource = nullptr;
  std::uint64_t id = 0;
  wl_resource* pendingBuffer = nullptr;
  wl_resource* currentBuffer = nullptr;
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t scale = 1;
  std::int32_t windowX = 96;
  std::int32_t windowY = 96;
  bool toplevel = false;
  bool cursor = false;
  std::uint64_t serial = 0;
  std::vector<std::uint8_t> rgbaPixels;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::int32_t frameWidth = 0;
  std::int32_t frameHeight = 0;
  bool snapped = false;
  std::int32_t restoreX = 96;
  std::int32_t restoreY = 96;
  std::int32_t restoreWidth = 0;
  std::int32_t restoreHeight = 0;
  DmabufBuffer* dmabufBuffer = nullptr;
  std::vector<wl_resource*> frameCallbacks;
};

struct WaylandServer::XdgSurface {
  WaylandServer* server = nullptr;
  wl_resource* resource = nullptr;
  Surface* surface = nullptr;
  bool configured = false;
};

struct WaylandServer::XdgToplevel {
  WaylandServer* server = nullptr;
  wl_resource* resource = nullptr;
  XdgSurface* xdgSurface = nullptr;
  std::string title;
  std::string appId;
};

struct WaylandServer::ShmPool {
  WaylandServer* server = nullptr;
  wl_resource* resource = nullptr;
  int fd = -1;
  std::int32_t size = 0;
  void* data = nullptr;
};

struct WaylandServer::ShmBuffer {
  WaylandServer* server = nullptr;
  wl_resource* resource = nullptr;
  ShmPool* pool = nullptr;
  std::int32_t offset = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::int32_t stride = 0;
  std::uint32_t format = 0;
};

struct DmabufPlane {
  int fd = -1;
  std::uint32_t index = 0;
  std::uint32_t offset = 0;
  std::uint32_t stride = 0;
  std::uint64_t modifier = DRM_FORMAT_MOD_INVALID;
};

struct WaylandServer::DmabufParams {
  WaylandServer* server = nullptr;
  wl_resource* resource = nullptr;
  std::vector<DmabufPlane> planes;
  bool used = false;
};

struct WaylandServer::DmabufBuffer {
  WaylandServer* server = nullptr;
  wl_resource* resource = nullptr;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::uint32_t format = 0;
  std::uint32_t flags = 0;
  std::vector<DmabufPlane> planes;
};

struct WaylandServer::ToplevelDecoration {
  WaylandServer* server = nullptr;
  wl_resource* resource = nullptr;
  XdgToplevel* toplevel = nullptr;
  std::uint32_t mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
};

namespace {

void surfaceDestroyResource(wl_resource* resource) {
  if (auto* surface = dataFrom<WaylandServer::Surface>(resource)) {
    surface->server->destroySurface(surface);
  }
}

void xdgSurfaceDestroyResource(wl_resource* resource) {
  if (auto* surface = dataFrom<WaylandServer::XdgSurface>(resource)) {
    surface->server->destroyXdgSurface(surface);
  }
}

void xdgToplevelDestroyResource(wl_resource* resource) {
  if (auto* toplevel = dataFrom<WaylandServer::XdgToplevel>(resource)) {
    toplevel->server->destroyXdgToplevel(toplevel);
  }
}

void shmPoolDestroyResource(wl_resource* resource) {
  if (auto* pool = dataFrom<WaylandServer::ShmPool>(resource)) {
    pool->server->destroyShmPool(pool);
  }
}

void shmBufferDestroyResource(wl_resource* resource) {
  if (auto* buffer = dataFrom<WaylandServer::ShmBuffer>(resource)) {
    buffer->server->destroyShmBuffer(buffer);
  }
}

void dmabufParamsDestroyResource(wl_resource* resource) {
  if (auto* params = dataFrom<WaylandServer::DmabufParams>(resource)) {
    params->server->destroyDmabufParams(params);
  }
}

void dmabufBufferDestroyResource(wl_resource* resource) {
  if (auto* buffer = dataFrom<WaylandServer::DmabufBuffer>(resource)) {
    buffer->server->destroyDmabufBuffer(buffer);
  }
}

void toplevelDecorationDestroyResource(wl_resource* resource) {
  if (auto* decoration = dataFrom<WaylandServer::ToplevelDecoration>(resource)) {
    decoration->server->destroyToplevelDecoration(decoration);
  }
}

void removeResource(std::vector<wl_resource*>& resources, wl_resource* resource) {
  resources.erase(std::remove(resources.begin(), resources.end(), resource), resources.end());
}

void seatDestroyResource(wl_resource* resource) {
  if (auto* server = serverFrom(resource)) removeResource(server->seatResources_, resource);
}

void pointerDestroyResource(wl_resource* resource) {
  if (auto* server = serverFrom(resource)) removeResource(server->pointerResources_, resource);
}

void keyboardDestroyResource(wl_resource* resource) {
  if (auto* server = serverFrom(resource)) removeResource(server->keyboardResources_, resource);
}

void bufferDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct wl_buffer_interface const bufferImpl{bufferDestroy};

void compositorCreateSurface(wl_client* client, wl_resource* resource, std::uint32_t id) {
  WaylandServer* server = serverFrom(resource);
  server->createSurface(client, wl_resource_get_version(resource), id);
}

void compositorCreateRegion(wl_client* client, wl_resource*, std::uint32_t id) {
  static struct wl_region_interface const regionImpl{
      inertDestroy,
      [](wl_client*, wl_resource*, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {},
      [](wl_client*, wl_resource*, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {},
  };
  wl_resource* region = wl_resource_create(client, &wl_region_interface, 1, id);
  wl_resource_set_implementation(region, &regionImpl, nullptr, nullptr);
}

struct wl_compositor_interface const compositorImpl{
    .create_surface = compositorCreateSurface,
    .create_region = compositorCreateRegion,
    .release = inertDestroy,
};

void surfaceDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void surfaceAttach(wl_client*, wl_resource* resource, wl_resource* buffer, std::int32_t x, std::int32_t y) {
  auto* surface = dataFrom<WaylandServer::Surface>(resource);
  surface->pendingBuffer = buffer;
  surface->x = x;
  surface->y = y;
}

void surfaceFrame(wl_client* client, wl_resource* resource, std::uint32_t id) {
  auto* surface = dataFrom<WaylandServer::Surface>(resource);
  wl_resource* callback = wl_resource_create(client, &wl_callback_interface, 1, id);
  if (!callback) {
    wl_client_post_no_memory(client);
    return;
  }
  surface->frameCallbacks.push_back(callback);
}

WaylandServer::ShmBuffer* shmBufferFor(WaylandServer* server, wl_resource* resource) {
  auto found = std::find_if(server->shmBuffers_.begin(), server->shmBuffers_.end(),
                            [resource](auto const& buffer) { return buffer->resource == resource; });
  return found == server->shmBuffers_.end() ? nullptr : found->get();
}

WaylandServer::DmabufBuffer* dmabufBufferFor(WaylandServer* server, wl_resource* resource) {
  auto found = std::find_if(server->dmabufBuffers_.begin(), server->dmabufBuffers_.end(),
                            [resource](auto const& buffer) { return buffer->resource == resource; });
  return found == server->dmabufBuffers_.end() ? nullptr : found->get();
}

bool copyShmBufferToRgba(WaylandServer::ShmBuffer const& buffer, std::vector<std::uint8_t>& out) {
  if (!buffer.pool || !buffer.pool->data || buffer.width <= 0 || buffer.height <= 0 || buffer.stride <= 0) {
    return false;
  }
  if (buffer.format != WL_SHM_FORMAT_ARGB8888 && buffer.format != WL_SHM_FORMAT_XRGB8888) {
    return false;
  }

  std::size_t const rowBytes = static_cast<std::size_t>(buffer.width) * 4u;
  if (buffer.offset < 0 || static_cast<std::size_t>(buffer.stride) < rowBytes) {
    return false;
  }
  std::size_t const lastRow = static_cast<std::size_t>(buffer.height - 1) * static_cast<std::size_t>(buffer.stride);
  std::size_t const end = static_cast<std::size_t>(buffer.offset) + lastRow + rowBytes;
  if (end > static_cast<std::size_t>(buffer.pool->size)) {
    return false;
  }

  out.resize(static_cast<std::size_t>(buffer.width) * static_cast<std::size_t>(buffer.height) * 4u);
  auto const* base = static_cast<std::uint8_t const*>(buffer.pool->data) + buffer.offset;
  for (std::int32_t y = 0; y < buffer.height; ++y) {
    auto const* src = base + static_cast<std::size_t>(y) * static_cast<std::size_t>(buffer.stride);
    auto* dst = out.data() + static_cast<std::size_t>(y) * rowBytes;
    for (std::int32_t x = 0; x < buffer.width; ++x) {
      // wl_shm ARGB/XRGB are little-endian words, so memory order is B, G, R, A/X.
      dst[static_cast<std::size_t>(x) * 4u + 0u] = src[static_cast<std::size_t>(x) * 4u + 2u];
      dst[static_cast<std::size_t>(x) * 4u + 1u] = src[static_cast<std::size_t>(x) * 4u + 1u];
      dst[static_cast<std::size_t>(x) * 4u + 2u] = src[static_cast<std::size_t>(x) * 4u + 0u];
      dst[static_cast<std::size_t>(x) * 4u + 3u] =
          buffer.format == WL_SHM_FORMAT_XRGB8888 ? 255u : src[static_cast<std::size_t>(x) * 4u + 3u];
    }
  }
  return true;
}

void surfaceCommit(wl_client*, wl_resource* resource) {
  auto* surface = dataFrom<WaylandServer::Surface>(resource);
  surface->currentBuffer = surface->pendingBuffer;
  if (surface->currentBuffer) {
    if (auto* shmBuffer = shmBufferFor(surface->server, surface->currentBuffer)) {
      std::vector<std::uint8_t> pixels;
      if (copyShmBufferToRgba(*shmBuffer, pixels)) {
        surface->rgbaPixels = std::move(pixels);
        surface->width = shmBuffer->width;
        surface->height = shmBuffer->height;
        if (surface->frameWidth <= 0) surface->frameWidth = surface->width;
        if (surface->frameHeight <= 0) surface->frameHeight = surface->height;
        surface->dmabufBuffer = nullptr;
        ++surface->serial;
      }
    } else if (auto* dmabufBuffer = dmabufBufferFor(surface->server, surface->currentBuffer)) {
      if (dmabufBuffer->width > 0 && dmabufBuffer->height > 0 && !dmabufBuffer->planes.empty()) {
        surface->rgbaPixels.clear();
        surface->width = dmabufBuffer->width;
        surface->height = dmabufBuffer->height;
        if (surface->frameWidth <= 0) surface->frameWidth = surface->width;
        if (surface->frameHeight <= 0) surface->frameHeight = surface->height;
        surface->dmabufBuffer = dmabufBuffer;
        ++surface->serial;
        std::fprintf(stderr,
                     "flux-compositor: received %dx%d DMABUF buffer format=0x%08x stride=%u modifier=0x%016llx\n",
                     dmabufBuffer->width,
                     dmabufBuffer->height,
                     dmabufBuffer->format,
                     dmabufBuffer->planes.front().stride,
                     static_cast<unsigned long long>(dmabufBuffer->planes.front().modifier));
      }
    }
    wl_buffer_send_release(surface->currentBuffer);
  } else {
    surface->rgbaPixels.clear();
    surface->width = 0;
    surface->height = 0;
    surface->frameWidth = 0;
    surface->frameHeight = 0;
    surface->dmabufBuffer = nullptr;
    ++surface->serial;
  }
}

void surfaceSetBufferScale(wl_client*, wl_resource* resource, std::int32_t scale) {
  auto* surface = dataFrom<WaylandServer::Surface>(resource);
  surface->scale = std::max(1, scale);
}

struct wl_surface_interface const surfaceImpl{
    .destroy = surfaceDestroy,
    .attach = surfaceAttach,
    .damage = [](wl_client*, wl_resource*, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {},
    .frame = surfaceFrame,
    .set_opaque_region = [](wl_client*, wl_resource*, wl_resource*) {},
    .set_input_region = [](wl_client*, wl_resource*, wl_resource*) {},
    .commit = surfaceCommit,
    .set_buffer_transform = [](wl_client*, wl_resource*, std::int32_t) {},
    .set_buffer_scale = surfaceSetBufferScale,
    .damage_buffer = [](wl_client*, wl_resource*, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {},
    .offset = [](wl_client*, wl_resource*, std::int32_t, std::int32_t) {},
    .get_release = surfaceFrame,
};

int createKeymapFd(std::uint32_t& size) {
  size = 0;
  xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!context) return -1;
  xkb_rule_names names{};
  xkb_keymap* keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap) {
    xkb_context_unref(context);
    return -1;
  }
  char* keymapString = xkb_keymap_get_as_string(keymap, XKB_KEYMAP_FORMAT_TEXT_V1);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  if (!keymapString) return -1;

  std::size_t const length = std::strlen(keymapString) + 1u;
  int fd = memfd_create("flux-compositor-keymap", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd >= 0 && ftruncate(fd, static_cast<off_t>(length)) == 0) {
    std::size_t written = 0;
    while (written < length) {
      ssize_t n = write(fd, keymapString + written, length - written);
      if (n <= 0) break;
      written += static_cast<std::size_t>(n);
    }
    if (written == length) {
      lseek(fd, 0, SEEK_SET);
      size = static_cast<std::uint32_t>(length);
      free(keymapString);
      return fd;
    }
  }
  if (fd >= 0) close(fd);
  free(keymapString);
  return -1;
}

void initializeKeyboardModifierIndices(WaylandServer* server) {
  xkb_context* context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if (!context) return;
  xkb_rule_names names{};
  xkb_keymap* keymap = xkb_keymap_new_from_names(context, &names, XKB_KEYMAP_COMPILE_NO_FLAGS);
  if (!keymap) {
    xkb_context_unref(context);
    return;
  }

  server->shiftModifierIndex_ = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_SHIFT);
  server->ctrlModifierIndex_ = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_CTRL);
  server->altModifierIndex_ = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_ALT);
  server->logoModifierIndex_ = xkb_keymap_mod_get_index(keymap, XKB_MOD_NAME_LOGO);

  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
}

void shmCreatePool(wl_client* client, wl_resource* resource, std::uint32_t id, int fd, std::int32_t size) {
  auto* server = serverFrom(resource);
  auto pool = std::make_unique<WaylandServer::ShmPool>();
  pool->server = server;
  pool->fd = fd;
  pool->size = size;
  if (size > 0) {
    pool->data = mmap(nullptr, static_cast<std::size_t>(size), PROT_READ, MAP_SHARED, fd, 0);
    if (pool->data == MAP_FAILED) pool->data = nullptr;
  }
  wl_resource* poolResource = wl_resource_create(client, &wl_shm_pool_interface, 1, id);
  pool->resource = poolResource;
  auto* raw = pool.get();
  server->shmPools_.push_back(std::move(pool));
  wl_resource_set_implementation(poolResource, &shmPoolImpl, raw, shmPoolDestroyResource);
}

struct wl_shm_interface const shmImpl{
    .create_pool = shmCreatePool,
    .release = inertDestroy,
};

void shmPoolCreateBuffer(wl_client* client, wl_resource* resource, std::uint32_t id, std::int32_t offset,
                         std::int32_t width, std::int32_t height, std::int32_t stride, std::uint32_t format) {
  auto* pool = dataFrom<WaylandServer::ShmPool>(resource);
  auto buffer = std::make_unique<WaylandServer::ShmBuffer>();
  buffer->server = pool->server;
  buffer->pool = pool;
  buffer->offset = offset;
  buffer->width = width;
  buffer->height = height;
  buffer->stride = stride;
  buffer->format = format;
  wl_resource* bufferResource = wl_resource_create(client, &wl_buffer_interface, 1, id);
  buffer->resource = bufferResource;
  auto* raw = buffer.get();
  pool->server->shmBuffers_.push_back(std::move(buffer));
  wl_resource_set_implementation(bufferResource, &bufferImpl, raw, shmBufferDestroyResource);
}

void shmPoolResize(wl_client*, wl_resource* resource, std::int32_t size) {
  auto* pool = dataFrom<WaylandServer::ShmPool>(resource);
  if (pool->data) {
    munmap(pool->data, static_cast<std::size_t>(pool->size));
    pool->data = nullptr;
  }
  pool->size = size;
  if (pool->fd >= 0 && size > 0) {
    pool->data = mmap(nullptr, static_cast<std::size_t>(size), PROT_READ, MAP_SHARED, pool->fd, 0);
    if (pool->data == MAP_FAILED) pool->data = nullptr;
  }
}

void shmPoolDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct wl_shm_pool_interface const shmPoolImpl{shmPoolCreateBuffer, shmPoolDestroy, shmPoolResize};

bool isSupportedDmabufFormat(std::uint32_t format) {
  return format == DRM_FORMAT_ARGB8888 || format == DRM_FORMAT_XRGB8888 ||
         format == DRM_FORMAT_ABGR8888 || format == DRM_FORMAT_XBGR8888;
}

std::optional<DmabufPlane> findPlane(WaylandServer::DmabufParams const* params, std::uint32_t index) {
  auto found = std::find_if(params->planes.begin(), params->planes.end(),
                            [index](DmabufPlane const& plane) { return plane.index == index; });
  if (found == params->planes.end()) return std::nullopt;
  return *found;
}

bool validateDmabufParams(WaylandServer::DmabufParams* params, std::int32_t width, std::int32_t height,
                          std::uint32_t format) {
  if (params->used) {
    wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                           "zwp_linux_buffer_params_v1 was already used");
    return false;
  }
  if (width <= 0 || height <= 0) {
    wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_DIMENSIONS,
                           "dmabuf dimensions must be positive");
    return false;
  }
  if (!isSupportedDmabufFormat(format)) {
    wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "unsupported dmabuf format 0x%08x", format);
    return false;
  }
  if (!findPlane(params, 0).has_value()) {
    wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                           "dmabuf plane 0 is required");
    return false;
  }
  return true;
}

wl_resource* createDmabufBuffer(wl_client* client, WaylandServer::DmabufParams* params, std::uint32_t id,
                                std::int32_t width, std::int32_t height, std::uint32_t format,
                                std::uint32_t flags) {
  auto buffer = std::make_unique<WaylandServer::DmabufBuffer>();
  buffer->server = params->server;
  buffer->width = width;
  buffer->height = height;
  buffer->format = format;
  buffer->flags = flags;
  buffer->planes = std::move(params->planes);
  wl_resource* bufferResource = wl_resource_create(client, &wl_buffer_interface, 1, id);
  if (!bufferResource) {
    for (auto& plane : buffer->planes) {
      if (plane.fd >= 0) close(plane.fd);
      plane.fd = -1;
    }
    wl_client_post_no_memory(client);
    return nullptr;
  }
  buffer->resource = bufferResource;
  auto* raw = buffer.get();
  params->server->dmabufBuffers_.push_back(std::move(buffer));
  wl_resource_set_implementation(bufferResource, &bufferImpl, raw, dmabufBufferDestroyResource);
  return bufferResource;
}

void linuxBufferParamsDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linuxBufferParamsAdd(wl_client*, wl_resource* resource, int fd, std::uint32_t planeIndex,
                          std::uint32_t offset, std::uint32_t stride, std::uint32_t modifierHi,
                          std::uint32_t modifierLo) {
  auto* params = dataFrom<WaylandServer::DmabufParams>(resource);
  if (params->used) {
    close(fd);
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_ALREADY_USED,
                           "zwp_linux_buffer_params_v1 was already used");
    return;
  }
  if (planeIndex >= 4) {
    close(fd);
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                           "dmabuf plane index %u is out of bounds", planeIndex);
    return;
  }
  if (findPlane(params, planeIndex).has_value()) {
    close(fd);
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                           "dmabuf plane index %u was already set", planeIndex);
    return;
  }

  DmabufPlane plane;
  plane.fd = fd;
  plane.index = planeIndex;
  plane.offset = offset;
  plane.stride = stride;
  plane.modifier = (static_cast<std::uint64_t>(modifierHi) << 32u) | modifierLo;
  params->planes.push_back(plane);
}

void linuxBufferParamsCreate(wl_client* client, wl_resource* resource, std::int32_t width,
                             std::int32_t height, std::uint32_t format, std::uint32_t flags) {
  auto* params = dataFrom<WaylandServer::DmabufParams>(resource);
  if (!validateDmabufParams(params, width, height, format)) return;
  params->used = true;
  wl_resource* buffer = createDmabufBuffer(client, params, 0, width, height, format, flags);
  if (buffer) zwp_linux_buffer_params_v1_send_created(resource, buffer);
}

void linuxBufferParamsCreateImmed(wl_client* client, wl_resource* resource, std::uint32_t bufferId,
                                  std::int32_t width, std::int32_t height, std::uint32_t format,
                                  std::uint32_t flags) {
  auto* params = dataFrom<WaylandServer::DmabufParams>(resource);
  if (!validateDmabufParams(params, width, height, format)) return;
  params->used = true;
  createDmabufBuffer(client, params, bufferId, width, height, format, flags);
}

struct zwp_linux_buffer_params_v1_interface const linuxBufferParamsImpl{
    .destroy = linuxBufferParamsDestroy,
    .add = linuxBufferParamsAdd,
    .create = linuxBufferParamsCreate,
    .create_immed = linuxBufferParamsCreateImmed,
};

void linuxDmabufDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linuxDmabufCreateParams(wl_client* client, wl_resource* resource, std::uint32_t id) {
  auto* server = serverFrom(resource);
  auto params = std::make_unique<WaylandServer::DmabufParams>();
  params->server = server;
  wl_resource* paramsResource = wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
                                                   wl_resource_get_version(resource), id);
  if (!paramsResource) {
    wl_client_post_no_memory(client);
    return;
  }
  params->resource = paramsResource;
  auto* raw = params.get();
  server->dmabufParams_.push_back(std::move(params));
  wl_resource_set_implementation(paramsResource, &linuxBufferParamsImpl, raw, dmabufParamsDestroyResource);
}

struct zwp_linux_dmabuf_v1_interface const linuxDmabufImpl{
    .destroy = linuxDmabufDestroy,
    .create_params = linuxDmabufCreateParams,
};

WaylandServer::ToplevelDecoration* decorationFor(WaylandServer* server, WaylandServer::XdgToplevel* toplevel) {
  auto found = std::find_if(server->toplevelDecorations_.begin(), server->toplevelDecorations_.end(),
                            [toplevel](auto const& decoration) { return decoration->toplevel == toplevel; });
  return found == server->toplevelDecorations_.end() ? nullptr : found->get();
}

WaylandServer::XdgToplevel* toplevelForSurface(WaylandServer* server, WaylandServer::Surface* surface) {
  auto found = std::find_if(server->toplevels_.begin(), server->toplevels_.end(),
                            [surface](auto const& toplevel) {
                              return toplevel->xdgSurface && toplevel->xdgSurface->surface == surface;
                            });
  return found == server->toplevels_.end() ? nullptr : found->get();
}

std::string titleForSurface(WaylandServer const* server, WaylandServer::Surface const* surface) {
  auto found = std::find_if(server->toplevels_.begin(), server->toplevels_.end(),
                            [surface](auto const& toplevel) {
                              return toplevel->xdgSurface && toplevel->xdgSurface->surface == surface;
                            });
  if (found == server->toplevels_.end()) return "Window";
  if (!(*found)->title.empty()) return (*found)->title;
  if (!(*found)->appId.empty()) return (*found)->appId;
  return "Window";
}

void sendToplevelConfigure(WaylandServer* server,
                           WaylandServer::XdgToplevel* toplevel,
                           std::int32_t width,
                           std::int32_t height) {
  if (!toplevel || !toplevel->resource || !toplevel->xdgSurface || !toplevel->xdgSurface->resource) return;
  wl_array states;
  wl_array_init(&states);
  xdg_toplevel_send_configure(toplevel->resource, width, height, &states);
  wl_array_release(&states);
  xdg_surface_send_configure(toplevel->xdgSurface->resource, server->nextConfigureSerial_++);
}

std::int32_t displayWidth(WaylandServer::Surface const* surface) {
  return surface && surface->frameWidth > 0 ? surface->frameWidth : surface ? surface->width : 0;
}

std::int32_t displayHeight(WaylandServer::Surface const* surface) {
  return surface && surface->frameHeight > 0 ? surface->frameHeight : surface ? surface->height : 0;
}

void sendDecorationConfigure(WaylandServer::ToplevelDecoration* decoration) {
  zxdg_toplevel_decoration_v1_send_configure(decoration->resource, decoration->mode);
  if (decoration->toplevel && decoration->toplevel->xdgSurface && decoration->toplevel->xdgSurface->resource) {
    xdg_surface_send_configure(decoration->toplevel->xdgSurface->resource,
                               decoration->server->nextConfigureSerial_++);
  }
}

void xdgDecorationManagerDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdgDecorationManagerGetToplevelDecoration(wl_client* client, wl_resource* resource, std::uint32_t id,
                                               wl_resource* toplevelResource) {
  auto* server = serverFrom(resource);
  auto* toplevel = dataFrom<WaylandServer::XdgToplevel>(toplevelResource);
  if (decorationFor(server, toplevel)) {
    wl_resource_post_error(resource, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_ALREADY_CONSTRUCTED,
                           "xdg_toplevel already has a decoration object");
    return;
  }

  auto decoration = std::make_unique<WaylandServer::ToplevelDecoration>();
  decoration->server = server;
  decoration->toplevel = toplevel;
  wl_resource* decorationResource = wl_resource_create(client, &zxdg_toplevel_decoration_v1_interface,
                                                       wl_resource_get_version(resource), id);
  if (!decorationResource) {
    wl_client_post_no_memory(client);
    return;
  }
  decoration->resource = decorationResource;
  auto* raw = decoration.get();
  server->toplevelDecorations_.push_back(std::move(decoration));
  wl_resource_set_implementation(decorationResource, &xdgToplevelDecorationImpl, raw,
                                 toplevelDecorationDestroyResource);
  sendDecorationConfigure(raw);
}

struct zxdg_decoration_manager_v1_interface const xdgDecorationManagerImpl{
    .destroy = xdgDecorationManagerDestroy,
    .get_toplevel_decoration = xdgDecorationManagerGetToplevelDecoration,
};

void xdgToplevelDecorationDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdgToplevelDecorationSetMode(wl_client*, wl_resource* resource, std::uint32_t mode) {
  auto* decoration = dataFrom<WaylandServer::ToplevelDecoration>(resource);
  if (mode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE && mode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE) {
    wl_resource_post_error(resource, ZXDG_TOPLEVEL_DECORATION_V1_ERROR_INVALID_MODE,
                           "invalid xdg-decoration mode %u", mode);
    return;
  }
  decoration->mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
  sendDecorationConfigure(decoration);
}

void xdgToplevelDecorationUnsetMode(wl_client*, wl_resource* resource) {
  auto* decoration = dataFrom<WaylandServer::ToplevelDecoration>(resource);
  decoration->mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
  sendDecorationConfigure(decoration);
}

struct zxdg_toplevel_decoration_v1_interface const xdgToplevelDecorationImpl{
    .destroy = xdgToplevelDecorationDestroy,
    .set_mode = xdgToplevelDecorationSetMode,
    .unset_mode = xdgToplevelDecorationUnsetMode,
};

void xdgWmBaseDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdgWmBaseCreatePositioner(wl_client* client, wl_resource* resource, std::uint32_t id) {
  static struct xdg_positioner_interface const positionerImpl{
      .destroy = inertDestroy,
      .set_size = [](wl_client*, wl_resource*, std::int32_t, std::int32_t) {},
      .set_anchor_rect = [](wl_client*, wl_resource*, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {},
      .set_anchor = [](wl_client*, wl_resource*, std::uint32_t) {},
      .set_gravity = [](wl_client*, wl_resource*, std::uint32_t) {},
      .set_constraint_adjustment = [](wl_client*, wl_resource*, std::uint32_t) {},
      .set_offset = [](wl_client*, wl_resource*, std::int32_t, std::int32_t) {},
      .set_reactive = [](wl_client*, wl_resource*) {},
      .set_parent_size = [](wl_client*, wl_resource*, std::int32_t, std::int32_t) {},
      .set_parent_configure = [](wl_client*, wl_resource*, std::uint32_t) {},
  };
  (void)resource;
  wl_resource* positioner = wl_resource_create(client, &xdg_positioner_interface, 6, id);
  wl_resource_set_implementation(positioner, &positionerImpl, nullptr, nullptr);
}

void xdgWmBaseGetXdgSurface(wl_client* client, wl_resource* resource, std::uint32_t id,
                            wl_resource* surfaceResource) {
  auto* server = serverFrom(resource);
  auto* surface = dataFrom<WaylandServer::Surface>(surfaceResource);
  auto xdgSurface = std::make_unique<WaylandServer::XdgSurface>();
  xdgSurface->server = server;
  xdgSurface->surface = surface;
  wl_resource* xdgResource = wl_resource_create(client, &xdg_surface_interface,
                                                wl_resource_get_version(resource), id);
  xdgSurface->resource = xdgResource;
  auto* raw = xdgSurface.get();
  server->xdgSurfaces_.push_back(std::move(xdgSurface));
  wl_resource_set_implementation(xdgResource, &xdgSurfaceImpl, raw, xdgSurfaceDestroyResource);
}

void xdgWmBasePong(wl_client*, wl_resource*, std::uint32_t) {}

struct xdg_wm_base_interface const xdgWmBaseImpl{
    .destroy = xdgWmBaseDestroy,
    .create_positioner = xdgWmBaseCreatePositioner,
    .get_xdg_surface = xdgWmBaseGetXdgSurface,
    .pong = xdgWmBasePong,
};

void xdgSurfaceDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdgSurfaceGetToplevel(wl_client* client, wl_resource* resource, std::uint32_t id) {
  auto* xdgSurface = dataFrom<WaylandServer::XdgSurface>(resource);
  auto toplevel = std::make_unique<WaylandServer::XdgToplevel>();
  toplevel->server = xdgSurface->server;
  toplevel->xdgSurface = xdgSurface;
  xdgSurface->surface->toplevel = true;
  xdgSurface->surface->cursor = false;
  if (xdgSurface->server->cursorSurface_ == xdgSurface->surface) xdgSurface->server->cursorSurface_ = nullptr;
  xdgSurface->surface->windowX = 80 + static_cast<std::int32_t>(xdgSurface->server->toplevels_.size()) * 36;
  xdgSurface->surface->windowY = 80 + static_cast<std::int32_t>(xdgSurface->server->toplevels_.size()) * 36;
  wl_resource* toplevelResource = wl_resource_create(client, &xdg_toplevel_interface,
                                                     wl_resource_get_version(resource), id);
  toplevel->resource = toplevelResource;
  auto* raw = toplevel.get();
  xdgSurface->server->toplevels_.push_back(std::move(toplevel));
  wl_resource_set_implementation(toplevelResource, &xdgToplevelImpl, raw, xdgToplevelDestroyResource);
  wl_array states;
  wl_array_init(&states);
  xdg_toplevel_send_configure(toplevelResource, 0, 0, &states);
  wl_array_release(&states);
  xdg_surface_send_configure(resource, xdgSurface->server->nextConfigureSerial_++);
}

void xdgSurfaceGetPopup(wl_client* client, wl_resource* resource, std::uint32_t id, wl_resource*, wl_resource*) {
  wl_resource_post_error(resource, XDG_WM_BASE_ERROR_INVALID_POSITIONER,
                         "xdg_popup is not implemented in phase 2");
  (void)client;
  (void)id;
}

void xdgSurfaceAckConfigure(wl_client*, wl_resource* resource, std::uint32_t) {
  dataFrom<WaylandServer::XdgSurface>(resource)->configured = true;
}

struct xdg_surface_interface const xdgSurfaceImpl{
    .destroy = xdgSurfaceDestroy,
    .get_toplevel = xdgSurfaceGetToplevel,
    .get_popup = xdgSurfaceGetPopup,
    .set_window_geometry = [](wl_client*, wl_resource*, std::int32_t, std::int32_t, std::int32_t, std::int32_t) {},
    .ack_configure = xdgSurfaceAckConfigure,
};

void xdgToplevelDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void xdgToplevelSetTitle(wl_client*, wl_resource* resource, char const* title) {
  dataFrom<WaylandServer::XdgToplevel>(resource)->title = title ? title : "";
}

void xdgToplevelSetAppId(wl_client*, wl_resource* resource, char const* appId) {
  dataFrom<WaylandServer::XdgToplevel>(resource)->appId = appId ? appId : "";
}

void xdgToplevelResize(wl_client*, wl_resource* resource, wl_resource*, std::uint32_t, std::uint32_t edges) {
  auto* toplevel = dataFrom<WaylandServer::XdgToplevel>(resource);
  if (!toplevel || !toplevel->xdgSurface || !toplevel->xdgSurface->surface) return;
  auto* server = toplevel->server;
  auto* surface = toplevel->xdgSurface->surface;
  std::int32_t const width = displayWidth(surface);
  std::int32_t const height = displayHeight(surface);
  if (edges == XDG_TOPLEVEL_RESIZE_EDGE_NONE || width <= 0 || height <= 0) return;
  server->resizeSurface_ = surface;
  server->resizeStartX_ = server->pointerX_;
  server->resizeStartY_ = server->pointerY_;
  server->resizeStartWindowX_ = surface->windowX;
  server->resizeStartWindowY_ = surface->windowY;
  server->resizeStartWidth_ = width;
  server->resizeStartHeight_ = height;
  server->resizeLastWidth_ = width;
  server->resizeLastHeight_ = height;
  server->resizeEdges_ = edges;
  surface->snapped = false;
}

struct xdg_toplevel_interface const xdgToplevelImpl{
    .destroy = xdgToplevelDestroy,
    .set_parent = [](wl_client*, wl_resource*, wl_resource*) {},
    .set_title = xdgToplevelSetTitle,
    .set_app_id = xdgToplevelSetAppId,
    .show_window_menu = [](wl_client*, wl_resource*, wl_resource*, std::uint32_t, std::int32_t, std::int32_t) {},
    .move = [](wl_client*, wl_resource*, wl_resource*, std::uint32_t) {},
    .resize = xdgToplevelResize,
    .set_max_size = [](wl_client*, wl_resource*, std::int32_t, std::int32_t) {},
    .set_min_size = [](wl_client*, wl_resource*, std::int32_t, std::int32_t) {},
    .set_maximized = [](wl_client*, wl_resource*) {},
    .unset_maximized = [](wl_client*, wl_resource*) {},
    .set_fullscreen = [](wl_client*, wl_resource*, wl_resource*) {},
    .unset_fullscreen = [](wl_client*, wl_resource*) {},
    .set_minimized = [](wl_client*, wl_resource*) {},
};

void bindCompositor(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &wl_compositor_interface, std::min(version, 5u), id);
  wl_resource_set_implementation(resource, &compositorImpl, data, nullptr);
}

void bindShm(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &wl_shm_interface, std::min(version, 1u), id);
  wl_resource_set_implementation(resource, &shmImpl, data, nullptr);
  wl_shm_send_format(resource, WL_SHM_FORMAT_ARGB8888);
  wl_shm_send_format(resource, WL_SHM_FORMAT_XRGB8888);
}

void bindOutput(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  auto* server = static_cast<WaylandServer*>(data);
  WaylandOutputInfo const& output = server->output_;
  wl_resource* resource = wl_resource_create(client, &wl_output_interface, std::min(version, 4u), id);
  static struct wl_output_interface const outputImpl{outputRelease};
  wl_resource_set_implementation(resource, &outputImpl, nullptr, nullptr);
  wl_output_send_geometry(resource, 0, 0, output.physicalWidthMm, output.physicalHeightMm,
                          WL_OUTPUT_SUBPIXEL_UNKNOWN, "Flux", output.name.c_str(),
                          WL_OUTPUT_TRANSFORM_NORMAL);
  wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                      output.width, output.height, output.refreshMilliHz);
  if (version >= 2) wl_output_send_scale(resource, 1);
  if (version >= 4) {
    wl_output_send_name(resource, output.name.c_str());
    wl_output_send_description(resource, "Flux compositor output");
  }
  if (version >= 2) wl_output_send_done(resource);
}

void pointerSetCursor(wl_client*, wl_resource* resource, std::uint32_t, wl_resource* surfaceResource,
                      std::int32_t hotspotX, std::int32_t hotspotY) {
  auto* server = serverFrom(resource);
  if (!surfaceResource) {
    server->cursorSurface_ = nullptr;
    return;
  }

  auto* surface = dataFrom<WaylandServer::Surface>(surfaceResource);
  if (!surface || surface->toplevel) return;
  surface->cursor = true;
  server->cursorSurface_ = surface;
  server->cursorHotspotX_ = hotspotX;
  server->cursorHotspotY_ = hotspotY;
}

void pointerRelease(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct wl_pointer_interface const pointerImpl{
    .set_cursor = pointerSetCursor,
    .release = pointerRelease,
};

void keyboardRelease(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct wl_keyboard_interface const keyboardImpl{
    .release = keyboardRelease,
};

void seatGetPointer(wl_client* client, wl_resource* resource, std::uint32_t id) {
  auto* server = serverFrom(resource);
  wl_resource* pointer = wl_resource_create(client, &wl_pointer_interface,
                                            std::min(wl_resource_get_version(resource), 7), id);
  if (!pointer) {
    wl_client_post_no_memory(client);
    return;
  }
  server->pointerResources_.push_back(pointer);
  wl_resource_set_implementation(pointer, &pointerImpl, server, pointerDestroyResource);
}

void seatGetKeyboard(wl_client* client, wl_resource* resource, std::uint32_t id) {
  auto* server = serverFrom(resource);
  wl_resource* keyboard = wl_resource_create(client, &wl_keyboard_interface,
                                             std::min(wl_resource_get_version(resource), 7), id);
  if (!keyboard) {
    wl_client_post_no_memory(client);
    return;
  }
  server->keyboardResources_.push_back(keyboard);
  wl_resource_set_implementation(keyboard, &keyboardImpl, server, keyboardDestroyResource);

  std::uint32_t keymapSize = 0;
  int keymapFd = createKeymapFd(keymapSize);
  if (keymapFd >= 0) {
    wl_keyboard_send_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, keymapFd, keymapSize);
    close(keymapFd);
  }
  if (wl_resource_get_version(keyboard) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION) {
    wl_keyboard_send_repeat_info(keyboard, 25, 600);
  }
}

void seatRelease(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct wl_seat_interface const seatImpl{
    .get_pointer = seatGetPointer,
    .get_keyboard = seatGetKeyboard,
    .get_touch = [](wl_client*, wl_resource*, std::uint32_t) {},
    .release = seatRelease,
};

void bindSeat(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &wl_seat_interface, std::min(version, 7u), id);
  auto* server = static_cast<WaylandServer*>(data);
  server->seatResources_.push_back(resource);
  wl_resource_set_implementation(resource, &seatImpl, server, seatDestroyResource);
  wl_seat_send_capabilities(resource, WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);
  if (version >= 2) wl_seat_send_name(resource, "seat0");
}

void bindXdgWmBase(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &xdg_wm_base_interface, std::min(version, 6u), id);
  wl_resource_set_implementation(resource, &xdgWmBaseImpl, data, nullptr);
}

void sendDmabufFormat(wl_resource* resource, std::uint32_t format) {
  if (wl_resource_get_version(resource) >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
    for (std::uint64_t modifier : {DRM_FORMAT_MOD_INVALID, DRM_FORMAT_MOD_LINEAR}) {
      zwp_linux_dmabuf_v1_send_modifier(resource, format, static_cast<std::uint32_t>(modifier >> 32u),
                                        static_cast<std::uint32_t>(modifier & 0xffffffffu));
    }
    return;
  }
  zwp_linux_dmabuf_v1_send_format(resource, format);
}

void bindLinuxDmabuf(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, std::min(version, 3u), id);
  wl_resource_set_implementation(resource, &linuxDmabufImpl, data, nullptr);
  sendDmabufFormat(resource, DRM_FORMAT_ARGB8888);
  sendDmabufFormat(resource, DRM_FORMAT_XRGB8888);
  sendDmabufFormat(resource, DRM_FORMAT_ABGR8888);
  sendDmabufFormat(resource, DRM_FORMAT_XBGR8888);
}

void bindXdgDecorationManager(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &zxdg_decoration_manager_v1_interface,
                                             std::min(version, 1u), id);
  wl_resource_set_implementation(resource, &xdgDecorationManagerImpl, data, nullptr);
}

} // namespace

WaylandServer::WaylandServer(WaylandOutputInfo output) : output_(std::move(output)) {
  initializeKeyboardModifierIndices(this);

  display_ = wl_display_create();
  if (!display_) throw std::runtime_error("wl_display_create failed");

  compositorGlobal_ = wl_global_create(display_, &wl_compositor_interface, 5, this, bindCompositor);
  shmGlobal_ = wl_global_create(display_, &wl_shm_interface, 1, this, bindShm);
  outputGlobal_ = wl_global_create(display_, &wl_output_interface, 4, this, bindOutput);
  seatGlobal_ = wl_global_create(display_, &wl_seat_interface, 7, this, bindSeat);
  xdgWmBaseGlobal_ = wl_global_create(display_, &xdg_wm_base_interface, 6, this, bindXdgWmBase);
  linuxDmabufGlobal_ = wl_global_create(display_, &zwp_linux_dmabuf_v1_interface, 3, this, bindLinuxDmabuf);
  xdgDecorationManagerGlobal_ =
      wl_global_create(display_, &zxdg_decoration_manager_v1_interface, 1, this, bindXdgDecorationManager);
  if (!compositorGlobal_ || !shmGlobal_ || !outputGlobal_ || !seatGlobal_ || !xdgWmBaseGlobal_ ||
      !linuxDmabufGlobal_ || !xdgDecorationManagerGlobal_) {
    throw std::runtime_error("failed to create Wayland globals");
  }

  char const* socket = wl_display_add_socket_auto(display_);
  if (!socket) throw std::runtime_error("wl_display_add_socket_auto failed");
  socketName_ = socket;
  setenv("WAYLAND_DISPLAY", socketName_.c_str(), 1);
  std::fprintf(stderr, "flux-compositor: Wayland display %s\n", socketName_.c_str());
}

WaylandServer::~WaylandServer() {
  if (!display_) return;
  wl_display_destroy_clients(display_);
  wl_display_destroy(display_);
}

char const* WaylandServer::socketName() const noexcept {
  return socketName_.c_str();
}

int WaylandServer::eventFd() const noexcept {
  return display_ ? wl_event_loop_get_fd(wl_display_get_event_loop(display_)) : -1;
}

std::size_t WaylandServer::toplevelCount() const noexcept {
  return toplevels_.size();
}

std::vector<CommittedSurfaceSnapshot> WaylandServer::committedSurfaces() const {
  std::vector<CommittedSurfaceSnapshot> snapshots;
  snapshots.reserve(surfaces_.size());
  for (auto const& surface : surfaces_) {
    if (!surface->toplevel) continue;
    if (surface->width <= 0 || surface->height <= 0) continue;
    if (surface->rgbaPixels.empty() && !surface->dmabufBuffer) continue;
    CommittedSurfaceSnapshot snapshot{
        .id = surface->id,
        .x = surface->windowX,
        .y = surface->windowY,
        .width = displayWidth(surface.get()),
        .height = displayHeight(surface.get()),
        .bufferWidth = surface->width,
        .bufferHeight = surface->height,
        .titleBarHeight = kTitleBarHeight,
        .title = titleForSurface(this, surface.get()),
        .focused = keyboardFocus_ == surface.get(),
        .serial = surface->serial,
        .rgbaPixels = surface->rgbaPixels,
        .dmabufFormat = 0,
        .dmabufPlanes = {},
    };
    if (surface->dmabufBuffer) {
      snapshot.dmabufFormat = surface->dmabufBuffer->format;
      snapshot.dmabufPlanes.reserve(surface->dmabufBuffer->planes.size());
      for (DmabufPlane const& plane : surface->dmabufBuffer->planes) {
        snapshot.dmabufPlanes.push_back({
            .offset = plane.offset,
            .stride = plane.stride,
            .modifier = plane.modifier,
        });
      }
    }
    snapshots.push_back(std::move(snapshot));
  }
  return snapshots;
}

std::optional<CommittedSurfaceSnapshot> WaylandServer::cursorSurface() const {
  Surface* surface = cursorSurface_;
  if (!surface || surface->width <= 0 || surface->height <= 0) return std::nullopt;
  if (surface->rgbaPixels.empty() && !surface->dmabufBuffer) return std::nullopt;

  CommittedSurfaceSnapshot snapshot{
      .id = surface->id,
      .x = static_cast<std::int32_t>(pointerX_) - cursorHotspotX_,
      .y = static_cast<std::int32_t>(pointerY_) - cursorHotspotY_,
      .width = surface->width,
      .height = surface->height,
      .bufferWidth = surface->width,
      .bufferHeight = surface->height,
      .serial = surface->serial,
      .rgbaPixels = surface->rgbaPixels,
      .dmabufFormat = 0,
      .dmabufPlanes = {},
  };
  if (surface->dmabufBuffer) {
    snapshot.dmabufFormat = surface->dmabufBuffer->format;
    snapshot.dmabufPlanes.reserve(surface->dmabufBuffer->planes.size());
    for (DmabufPlane const& plane : surface->dmabufBuffer->planes) {
      snapshot.dmabufPlanes.push_back({
          .offset = plane.offset,
          .stride = plane.stride,
          .modifier = plane.modifier,
      });
    }
  }
  return snapshot;
}

std::vector<int> WaylandServer::duplicateDmabufFds(std::uint64_t surfaceId) const {
  auto surface = std::find_if(surfaces_.begin(), surfaces_.end(),
                              [surfaceId](auto const& candidate) { return candidate->id == surfaceId; });
  if (surface == surfaces_.end() || !(*surface)->dmabufBuffer) return {};

  std::vector<int> fds;
  fds.reserve((*surface)->dmabufBuffer->planes.size());
  for (DmabufPlane const& plane : (*surface)->dmabufBuffer->planes) {
    int copied = dup(plane.fd);
    if (copied < 0) {
      for (int fd : fds) close(fd);
      return {};
    }
    fds.push_back(copied);
  }
  return fds;
}

WaylandServer::Surface* surfaceAt(WaylandServer* server, float x, float y) {
  for (auto it = server->surfaces_.rbegin(); it != server->surfaces_.rend(); ++it) {
    WaylandServer::Surface* surface = it->get();
    std::int32_t const width = displayWidth(surface);
    std::int32_t const height = displayHeight(surface);
    if (!surface || !surface->toplevel || width <= 0 || height <= 0) continue;
    float const left = static_cast<float>(surface->windowX);
    float const top = static_cast<float>(surface->windowY);
    float const right = left + static_cast<float>(width);
    float const bottom = top + static_cast<float>(height);
    if (x >= left && x < right && y >= top && y < bottom) return surface;
  }
  return nullptr;
}

WaylandServer::Surface* titlebarAt(WaylandServer* server, float x, float y) {
  for (auto it = server->surfaces_.rbegin(); it != server->surfaces_.rend(); ++it) {
    WaylandServer::Surface* surface = it->get();
    std::int32_t const width = displayWidth(surface);
    std::int32_t const height = displayHeight(surface);
    if (!surface || !surface->toplevel || width <= 0 || height <= 0) continue;
    float const left = static_cast<float>(surface->windowX);
    float const top = static_cast<float>(surface->windowY - kTitleBarHeight);
    float const right = left + static_cast<float>(width);
    float const bottom = static_cast<float>(surface->windowY);
    if (x >= left && x < right && y >= top && y < bottom) return surface;
  }
  return nullptr;
}

WaylandServer::Surface* closeButtonAt(WaylandServer* server, float x, float y) {
  for (auto it = server->surfaces_.rbegin(); it != server->surfaces_.rend(); ++it) {
    WaylandServer::Surface* surface = it->get();
    std::int32_t const width = displayWidth(surface);
    std::int32_t const height = displayHeight(surface);
    if (!surface || !surface->toplevel || width <= 0 || height <= 0) continue;
    float const left = static_cast<float>(surface->windowX + width - kCloseButtonInset - kCloseButtonSize);
    float const top = static_cast<float>(surface->windowY - kTitleBarHeight + kCloseButtonInset);
    float const right = left + static_cast<float>(kCloseButtonSize);
    float const bottom = top + static_cast<float>(kCloseButtonSize);
    if (x >= left && x < right && y >= top && y < bottom) return surface;
  }
  return nullptr;
}

WaylandServer::Surface* resizeGripAt(WaylandServer* server, float x, float y, std::uint32_t& edges) {
  edges = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
  for (auto it = server->surfaces_.rbegin(); it != server->surfaces_.rend(); ++it) {
    WaylandServer::Surface* surface = it->get();
    std::int32_t const width = displayWidth(surface);
    std::int32_t const height = displayHeight(surface);
    if (!surface || !surface->toplevel || width <= 0 || height <= 0) continue;
    float const left = static_cast<float>(surface->windowX);
    float const top = static_cast<float>(surface->windowY);
    float const right = left + static_cast<float>(width);
    float const bottom = top + static_cast<float>(height);
    bool const nearLeft = x >= left && x < left + kResizeGripSize;
    bool const nearRight = x >= right - kResizeGripSize && x < right;
    bool const nearTop = y >= top && y < top + kResizeGripSize;
    bool const nearBottom = y >= bottom - kResizeGripSize && y < bottom;
    if (nearLeft && nearTop) edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
    else if (nearRight && nearTop) edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
    else if (nearLeft && nearBottom) edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
    else if (nearRight && nearBottom) edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
    else continue;
    return surface;
  }
  return nullptr;
}

void raiseSurface(WaylandServer* server, WaylandServer::Surface* surface) {
  auto found = std::find_if(server->surfaces_.begin(), server->surfaces_.end(),
                            [surface](auto const& candidate) { return candidate.get() == surface; });
  if (found == server->surfaces_.end() || std::next(found) == server->surfaces_.end()) return;
  auto item = std::move(*found);
  server->surfaces_.erase(found);
  server->surfaces_.push_back(std::move(item));
}

void sendPointerFocus(WaylandServer* server, WaylandServer::Surface* next, std::uint32_t timeMs) {
  if (server->pointerFocus_ == next) {
    if (!next) return;
    wl_fixed_t const x = wl_fixed_from_double(server->pointerX_ - static_cast<float>(next->windowX));
    wl_fixed_t const y = wl_fixed_from_double(server->pointerY_ - static_cast<float>(next->windowY));
    for (wl_resource* pointer : server->pointerResources_) {
      wl_pointer_send_motion(pointer, timeMs, x, y);
      if (wl_resource_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION) wl_pointer_send_frame(pointer);
    }
    return;
  }

  std::uint32_t serial = server->nextInputSerial_++;
  if (server->pointerFocus_) {
    for (wl_resource* pointer : server->pointerResources_) {
      wl_pointer_send_leave(pointer, serial, server->pointerFocus_->resource);
      if (wl_resource_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION) wl_pointer_send_frame(pointer);
    }
  }
  server->pointerFocus_ = next;
  if (next) {
    wl_fixed_t const x = wl_fixed_from_double(server->pointerX_ - static_cast<float>(next->windowX));
    wl_fixed_t const y = wl_fixed_from_double(server->pointerY_ - static_cast<float>(next->windowY));
    for (wl_resource* pointer : server->pointerResources_) {
      wl_pointer_send_enter(pointer, serial, next->resource, x, y);
      if (wl_resource_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION) wl_pointer_send_frame(pointer);
    }
  }
}

std::uint32_t keyboardModifierMask(WaylandServer* server);

void setKeyboardFocus(WaylandServer* server, WaylandServer::Surface* next) {
  if (server->keyboardFocus_ == next) return;
  std::uint32_t serial = server->nextInputSerial_++;
  if (server->keyboardFocus_) {
    for (wl_resource* keyboard : server->keyboardResources_) {
      wl_keyboard_send_leave(keyboard, serial, server->keyboardFocus_->resource);
    }
  }
  server->keyboardFocus_ = next;
  if (next) {
    wl_array keys;
    wl_array_init(&keys);
    std::uint32_t const modifiers = keyboardModifierMask(server);
    for (wl_resource* keyboard : server->keyboardResources_) {
      wl_keyboard_send_enter(keyboard, serial, next->resource, &keys);
      wl_keyboard_send_modifiers(keyboard, server->nextInputSerial_++, modifiers, 0, 0, 0);
    }
    wl_array_release(&keys);
  }
}

std::uint32_t modifierBit(std::uint32_t index, bool active) {
  if (!active || index == kInvalidModifierIndex || index >= 32u) return 0u;
  return 1u << index;
}

std::uint32_t keyboardModifierMask(WaylandServer* server) {
  return modifierBit(server->shiftModifierIndex_, server->shiftDown_) |
         modifierBit(server->ctrlModifierIndex_, server->ctrlDown_) |
         modifierBit(server->altModifierIndex_, server->altDown_) |
         modifierBit(server->logoModifierIndex_, server->metaDown_);
}

void sendKeyboardModifiers(WaylandServer* server) {
  std::uint32_t const depressed = keyboardModifierMask(server);
  for (wl_resource* keyboard : server->keyboardResources_) {
    wl_keyboard_send_modifiers(keyboard, server->nextInputSerial_++, depressed, 0, 0, 0);
  }
}

void focusSurface(WaylandServer* server, WaylandServer::Surface* surface, std::uint32_t timeMs) {
  if (!surface) return;
  raiseSurface(server, surface);
  setKeyboardFocus(server, surface);
  sendPointerFocus(server, surfaceAt(server, server->pointerX_, server->pointerY_), timeMs);
}

WaylandServer::Surface* previousToplevel(WaylandServer* server, WaylandServer::Surface* current) {
  WaylandServer::Surface* previous = nullptr;
  for (auto const& surface : server->surfaces_) {
    if (!surface || !surface->toplevel) continue;
    if (surface.get() == current) return previous;
    previous = surface.get();
  }
  return previous;
}

WaylandServer::XdgToplevel* focusedToplevel(WaylandServer* server) {
  return toplevelForSurface(server, server->keyboardFocus_);
}

bool closeFocusedToplevel(WaylandServer* server) {
  WaylandServer::XdgToplevel* toplevel = focusedToplevel(server);
  if (!toplevel || !toplevel->resource) return false;
  xdg_toplevel_send_close(toplevel->resource);
  return true;
}

bool cycleFocus(WaylandServer* server, std::uint32_t timeMs) {
  WaylandServer::Surface* target = previousToplevel(server, server->keyboardFocus_);
  if (!target) {
    for (auto const& surface : server->surfaces_) {
      if (surface && surface->toplevel) {
        target = surface.get();
      }
    }
  }
  if (!target || target == server->keyboardFocus_) return false;
  focusSurface(server, target, timeMs);
  return true;
}

void snapFocusedToplevel(WaylandServer* server, bool leftHalf) {
  WaylandServer::Surface* surface = server->keyboardFocus_;
  if (!surface || !surface->toplevel) return;
  if (!surface->snapped) {
    surface->restoreX = surface->windowX;
    surface->restoreY = surface->windowY;
    surface->restoreWidth = displayWidth(surface);
    surface->restoreHeight = displayHeight(surface);
  }
  std::int32_t const width = std::max(kMinWindowWidth, server->output_.width / 2);
  std::int32_t const height = std::max(kMinWindowHeight, server->output_.height - kTitleBarHeight);
  surface->windowX = leftHalf ? 0 : std::max(0, server->output_.width - width);
  surface->windowY = kTitleBarHeight;
  surface->frameWidth = width;
  surface->frameHeight = height;
  surface->snapped = true;
  sendToplevelConfigure(server, toplevelForSurface(server, surface), width, height);
}

void restoreSnappedForDrag(WaylandServer* server, WaylandServer::Surface* surface) {
  if (!surface || !surface->snapped) return;
  std::int32_t const snappedWidth = std::max(1, displayWidth(surface));
  std::int32_t const restoreWidth =
      std::max(kMinWindowWidth, surface->restoreWidth > 0 ? surface->restoreWidth : surface->width);
  std::int32_t const restoreHeight =
      std::max(kMinWindowHeight, surface->restoreHeight > 0 ? surface->restoreHeight : surface->height);
  float const horizontalRatio =
      std::clamp((server->pointerX_ - static_cast<float>(surface->windowX)) / static_cast<float>(snappedWidth),
                 0.f,
                 1.f);
  int const maxX = std::max(0, server->output_.width - restoreWidth);
  int const maxY = std::max(kTitleBarHeight, server->output_.height - restoreHeight);
  surface->windowX = std::clamp(static_cast<int>(std::lround(server->pointerX_ -
                                                             horizontalRatio * static_cast<float>(restoreWidth))),
                                0,
                                maxX);
  surface->windowY =
      std::clamp(static_cast<int>(std::lround(server->pointerY_ - server->dragOffsetY_)), kTitleBarHeight, maxY);
  surface->frameWidth = restoreWidth;
  surface->frameHeight = restoreHeight;
  surface->snapped = false;
  server->dragOffsetX_ = server->pointerX_ - static_cast<float>(surface->windowX);
  server->dragOffsetY_ = server->pointerY_ - static_cast<float>(surface->windowY);
  sendToplevelConfigure(server, toplevelForSurface(server, surface), restoreWidth, restoreHeight);
}

bool updateShortcutModifier(WaylandServer* server, std::uint32_t key, bool pressed) {
  bool changed = false;
  if (key == KEY_LEFTMETA || key == KEY_RIGHTMETA) {
    changed = server->metaDown_ != pressed;
    server->metaDown_ = pressed;
    if (changed) sendKeyboardModifiers(server);
    return true;
  }
  if (key == KEY_LEFTCTRL || key == KEY_RIGHTCTRL) {
    changed = server->ctrlDown_ != pressed;
    server->ctrlDown_ = pressed;
    if (changed) sendKeyboardModifiers(server);
    return false;
  }
  if (key == KEY_LEFTALT || key == KEY_RIGHTALT) {
    changed = server->altDown_ != pressed;
    server->altDown_ = pressed;
    if (changed) sendKeyboardModifiers(server);
    return false;
  }
  if (key == KEY_LEFTSHIFT || key == KEY_RIGHTSHIFT) {
    changed = server->shiftDown_ != pressed;
    server->shiftDown_ = pressed;
    if (changed) sendKeyboardModifiers(server);
    return false;
  }
  return false;
}

bool handleCompositorShortcut(WaylandServer* server, std::uint32_t key, bool pressed, std::uint32_t timeMs) {
  if (!pressed) return false;
  if (server->metaDown_) {
    if (key == KEY_Q) return closeFocusedToplevel(server);
    if (key == KEY_TAB) return cycleFocus(server, timeMs);
    if (key == KEY_LEFT) {
      snapFocusedToplevel(server, true);
      return true;
    }
    if (key == KEY_RIGHT) {
      snapFocusedToplevel(server, false);
      return true;
    }
  }
  if (server->ctrlDown_ && server->altDown_ && key == KEY_BACKSPACE) {
    std::raise(SIGTERM);
    return true;
  }
  return false;
}

void updateDrag(WaylandServer* server) {
  if (!server->dragSurface_) return;
  WaylandServer::Surface* surface = server->dragSurface_;
  restoreSnappedForDrag(server, surface);
  int const maxX = std::max(0, server->output_.width - displayWidth(surface));
  int const maxY = std::max(kTitleBarHeight, server->output_.height - displayHeight(surface));
  surface->windowX = std::clamp(static_cast<int>(server->pointerX_ - server->dragOffsetX_), 0, maxX);
  surface->windowY = std::clamp(static_cast<int>(server->pointerY_ - server->dragOffsetY_), kTitleBarHeight, maxY);
}

void updateResize(WaylandServer* server) {
  WaylandServer::Surface* surface = server->resizeSurface_;
  if (!surface) return;

  float const dx = server->pointerX_ - server->resizeStartX_;
  float const dy = server->pointerY_ - server->resizeStartY_;
  bool const left = server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_LEFT ||
                    server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT ||
                    server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
  bool const right = server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_RIGHT ||
                     server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT ||
                     server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
  bool const top = server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_TOP ||
                   server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT ||
                   server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
  bool const bottom = server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM ||
                      server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT ||
                      server->resizeEdges_ == XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;

  std::int32_t nextWidth = server->resizeStartWidth_;
  std::int32_t nextHeight = server->resizeStartHeight_;
  std::int32_t nextX = server->resizeStartWindowX_;
  std::int32_t nextY = server->resizeStartWindowY_;
  if (right) nextWidth = server->resizeStartWidth_ + static_cast<std::int32_t>(std::lround(dx));
  if (bottom) nextHeight = server->resizeStartHeight_ + static_cast<std::int32_t>(std::lround(dy));
  if (left) {
    nextWidth = server->resizeStartWidth_ - static_cast<std::int32_t>(std::lround(dx));
    nextX = server->resizeStartWindowX_ + (server->resizeStartWidth_ - nextWidth);
  }
  if (top) {
    nextHeight = server->resizeStartHeight_ - static_cast<std::int32_t>(std::lround(dy));
    nextY = server->resizeStartWindowY_ + (server->resizeStartHeight_ - nextHeight);
  }

  nextWidth = std::max(kMinWindowWidth, nextWidth);
  nextHeight = std::max(kMinWindowHeight, nextHeight);
  if (left) nextX = server->resizeStartWindowX_ + (server->resizeStartWidth_ - nextWidth);
  if (top) nextY = server->resizeStartWindowY_ + (server->resizeStartHeight_ - nextHeight);
  nextX = std::clamp(nextX, 0, std::max(0, server->output_.width - kMinWindowWidth));
  nextY = std::clamp(nextY, kTitleBarHeight, std::max(kTitleBarHeight, server->output_.height - kMinWindowHeight));

  if (left) surface->windowX = nextX;
  if (top) surface->windowY = nextY;
  if (nextWidth == server->resizeLastWidth_ && nextHeight == server->resizeLastHeight_) return;
  server->resizeLastWidth_ = nextWidth;
  server->resizeLastHeight_ = nextHeight;
  surface->frameWidth = nextWidth;
  surface->frameHeight = nextHeight;
  sendToplevelConfigure(server, toplevelForSurface(server, surface), nextWidth, nextHeight);
}

void WaylandServer::handlePointerMotion(double dx, double dy, std::uint32_t timeMs) {
  pointerX_ = std::clamp(pointerX_ + static_cast<float>(dx), 0.f, std::max(0.f, static_cast<float>(output_.width - 1)));
  pointerY_ = std::clamp(pointerY_ + static_cast<float>(dy), 0.f, std::max(0.f, static_cast<float>(output_.height - 1)));
  if (resizeSurface_) {
    updateResize(this);
    return;
  }
  if (dragSurface_) {
    updateDrag(this);
    return;
  }
  sendPointerFocus(this, surfaceAt(this, pointerX_, pointerY_), timeMs);
}

void WaylandServer::handlePointerPosition(double x, double y, std::uint32_t timeMs) {
  pointerX_ = std::clamp(static_cast<float>(x), 0.f, std::max(0.f, static_cast<float>(output_.width - 1)));
  pointerY_ = std::clamp(static_cast<float>(y), 0.f, std::max(0.f, static_cast<float>(output_.height - 1)));
  if (resizeSurface_) {
    updateResize(this);
    return;
  }
  if (dragSurface_) {
    updateDrag(this);
    return;
  }
  sendPointerFocus(this, surfaceAt(this, pointerX_, pointerY_), timeMs);
}

void WaylandServer::handlePointerButton(std::uint32_t button, bool pressed, std::uint32_t timeMs) {
  Surface* target = surfaceAt(this, pointerX_, pointerY_);
  if (button == BTN_LEFT) {
    if (pressed) {
      if (Surface* closeTarget = closeButtonAt(this, pointerX_, pointerY_)) {
        raiseSurface(this, closeTarget);
        setKeyboardFocus(this, closeTarget);
        closePressSurface_ = closeTarget;
        return;
      }
      std::uint32_t resizeEdges = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
      Surface* resizeTarget = resizeGripAt(this, pointerX_, pointerY_, resizeEdges);
      if (resizeTarget) {
        raiseSurface(this, resizeTarget);
        setKeyboardFocus(this, resizeTarget);
        sendPointerFocus(this, nullptr, timeMs);
        resizeTarget->snapped = false;
        resizeSurface_ = resizeTarget;
        resizeStartX_ = pointerX_;
        resizeStartY_ = pointerY_;
        resizeStartWindowX_ = resizeTarget->windowX;
        resizeStartWindowY_ = resizeTarget->windowY;
        resizeStartWidth_ = displayWidth(resizeTarget);
        resizeStartHeight_ = displayHeight(resizeTarget);
        resizeLastWidth_ = resizeStartWidth_;
        resizeLastHeight_ = resizeStartHeight_;
        resizeEdges_ = resizeEdges;
        return;
      }
      Surface* chromeTarget = titlebarAt(this, pointerX_, pointerY_);
      if (chromeTarget) {
        raiseSurface(this, chromeTarget);
        setKeyboardFocus(this, chromeTarget);
        sendPointerFocus(this, nullptr, timeMs);
        dragSurface_ = chromeTarget;
        dragOffsetX_ = pointerX_ - static_cast<float>(chromeTarget->windowX);
        dragOffsetY_ = pointerY_ - static_cast<float>(chromeTarget->windowY);
        return;
      }
    } else if (closePressSurface_) {
      Surface* closeTarget = closeButtonAt(this, pointerX_, pointerY_);
      if (closeTarget && closeTarget == closePressSurface_) {
        setKeyboardFocus(this, closePressSurface_);
        closeFocusedToplevel(this);
        flushClients();
      }
      closePressSurface_ = nullptr;
      return;
    } else if (resizeSurface_) {
      updateResize(this);
      resizeSurface_ = nullptr;
      resizeEdges_ = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
      return;
    } else if (dragSurface_) {
      dragSurface_ = nullptr;
      return;
    }
  }

  if (pressed && target) {
    raiseSurface(this, target);
    setKeyboardFocus(this, target);
    sendPointerFocus(this, target, timeMs);
  }
  if (!pointerFocus_) return;
  std::uint32_t serial = nextInputSerial_++;
  for (wl_resource* pointer : pointerResources_) {
    wl_pointer_send_button(pointer,
                           serial,
                           timeMs,
                           button,
                           pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
    if (wl_resource_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION) wl_pointer_send_frame(pointer);
  }
}

void WaylandServer::handlePointerAxis(double dx, double dy, std::uint32_t timeMs) {
  if (!pointerFocus_) return;
  for (wl_resource* pointer : pointerResources_) {
    if (dx != 0.0) {
      wl_pointer_send_axis(pointer, timeMs, WL_POINTER_AXIS_HORIZONTAL_SCROLL, wl_fixed_from_double(dx));
    }
    if (dy != 0.0) {
      wl_pointer_send_axis(pointer, timeMs, WL_POINTER_AXIS_VERTICAL_SCROLL, wl_fixed_from_double(dy));
    }
    if (wl_resource_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION) wl_pointer_send_frame(pointer);
  }
}

void WaylandServer::handleKeyboardKey(std::uint32_t key, bool pressed, std::uint32_t timeMs) {
  bool const consumeModifier = updateShortcutModifier(this, key, pressed);
  if (consumeModifier) return;
  if (handleCompositorShortcut(this, key, pressed, timeMs)) return;
  if (!keyboardFocus_) return;
  std::uint32_t serial = nextInputSerial_++;
  for (wl_resource* keyboard : keyboardResources_) {
    wl_keyboard_send_key(keyboard,
                         serial,
                         timeMs,
                         key,
                         pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED);
  }
}

bool WaylandServer::copyDmabufToRgba(std::uint64_t surfaceId, std::vector<std::uint8_t>& out) const {
  auto surface = std::find_if(surfaces_.begin(), surfaces_.end(),
                              [surfaceId](auto const& candidate) { return candidate->id == surfaceId; });
  if (surface == surfaces_.end() || !(*surface)->dmabufBuffer) return false;

  DmabufBuffer const& buffer = *(*surface)->dmabufBuffer;
  if (buffer.width <= 0 || buffer.height <= 0 || buffer.planes.size() != 1) return false;
  if (!isSupportedDmabufFormat(buffer.format)) return false;

  DmabufPlane const& plane = buffer.planes.front();
  if (plane.fd < 0 || plane.stride < static_cast<std::uint32_t>(buffer.width) * 4u) return false;
  if (plane.modifier != DRM_FORMAT_MOD_LINEAR && plane.modifier != DRM_FORMAT_MOD_INVALID) return false;

  std::size_t const rowBytes = static_cast<std::size_t>(buffer.width) * 4u;
  std::size_t const dataSize = static_cast<std::size_t>(plane.offset) +
                               static_cast<std::size_t>(plane.stride) *
                                   static_cast<std::size_t>(buffer.height);
  void* mapping = mmap(nullptr, dataSize, PROT_READ, MAP_SHARED, plane.fd, 0);
  if (mapping == MAP_FAILED) {
    std::fprintf(stderr, "flux-compositor: dmabuf CPU fallback mmap failed: %s\n", std::strerror(errno));
    return false;
  }

  out.resize(static_cast<std::size_t>(buffer.width) * static_cast<std::size_t>(buffer.height) * 4u);
  auto const* base = static_cast<std::uint8_t const*>(mapping) + plane.offset;
  for (std::int32_t y = 0; y < buffer.height; ++y) {
    auto const* src = base + static_cast<std::size_t>(y) * plane.stride;
    auto* dst = out.data() + static_cast<std::size_t>(y) * rowBytes;
    for (std::int32_t x = 0; x < buffer.width; ++x) {
      std::uint8_t const b0 = src[static_cast<std::size_t>(x) * 4u + 0u];
      std::uint8_t const b1 = src[static_cast<std::size_t>(x) * 4u + 1u];
      std::uint8_t const b2 = src[static_cast<std::size_t>(x) * 4u + 2u];
      std::uint8_t const b3 = src[static_cast<std::size_t>(x) * 4u + 3u];
      if (buffer.format == DRM_FORMAT_ARGB8888 || buffer.format == DRM_FORMAT_XRGB8888) {
        dst[static_cast<std::size_t>(x) * 4u + 0u] = b2;
        dst[static_cast<std::size_t>(x) * 4u + 1u] = b1;
        dst[static_cast<std::size_t>(x) * 4u + 2u] = b0;
        dst[static_cast<std::size_t>(x) * 4u + 3u] =
            buffer.format == DRM_FORMAT_XRGB8888 ? 255u : b3;
      } else {
        dst[static_cast<std::size_t>(x) * 4u + 0u] = b0;
        dst[static_cast<std::size_t>(x) * 4u + 1u] = b1;
        dst[static_cast<std::size_t>(x) * 4u + 2u] = b2;
        dst[static_cast<std::size_t>(x) * 4u + 3u] =
            buffer.format == DRM_FORMAT_XBGR8888 ? 255u : b3;
      }
    }
  }

  munmap(mapping, dataSize);
  return true;
}

void WaylandServer::dispatch() {
  if (!display_) return;
  wl_event_loop_dispatch(wl_display_get_event_loop(display_), 0);
  wl_display_flush_clients(display_);
}

void WaylandServer::flushClients() {
  if (display_) wl_display_flush_clients(display_);
}

void WaylandServer::sendFrameCallbacks(std::uint32_t timeMs) {
  for (auto const& surface : surfaces_) {
    std::vector<wl_resource*> callbacks = std::move(surface->frameCallbacks);
    surface->frameCallbacks.clear();
    for (wl_resource* callback : callbacks) {
      wl_callback_send_done(callback, timeMs);
      wl_resource_destroy(callback);
    }
  }
  flushClients();
}

wl_resource* WaylandServer::createSurface(wl_client* client, std::uint32_t version, std::uint32_t id) {
  auto surface = std::make_unique<Surface>();
  surface->server = this;
  surface->id = nextSurfaceId_++;
  wl_resource* resource = wl_resource_create(client, &wl_surface_interface, std::min(version, 5u), id);
  surface->resource = resource;
  auto* raw = surface.get();
  surfaces_.push_back(std::move(surface));
  wl_resource_set_implementation(resource, &surfaceImpl, raw, surfaceDestroyResource);
  return resource;
}

void WaylandServer::destroySurface(Surface* surface) {
  if (pointerFocus_ == surface) pointerFocus_ = nullptr;
  if (keyboardFocus_ == surface) keyboardFocus_ = nullptr;
  if (dragSurface_ == surface) dragSurface_ = nullptr;
  if (resizeSurface_ == surface) resizeSurface_ = nullptr;
  if (closePressSurface_ == surface) closePressSurface_ = nullptr;
  if (cursorSurface_ == surface) cursorSurface_ = nullptr;
  for (wl_resource* callback : surface->frameCallbacks) {
    wl_resource_destroy(callback);
  }
  surface->frameCallbacks.clear();
  surfaces_.erase(std::remove_if(surfaces_.begin(), surfaces_.end(),
                                 [&](auto const& candidate) { return candidate.get() == surface; }),
                  surfaces_.end());
}

void WaylandServer::destroyXdgSurface(XdgSurface* surface) {
  xdgSurfaces_.erase(std::remove_if(xdgSurfaces_.begin(), xdgSurfaces_.end(),
                                    [&](auto const& candidate) { return candidate.get() == surface; }),
                     xdgSurfaces_.end());
}

void WaylandServer::destroyXdgToplevel(XdgToplevel* toplevel) {
  while (auto* decoration = decorationFor(this, toplevel)) {
    wl_resource_destroy(decoration->resource);
  }
  toplevels_.erase(std::remove_if(toplevels_.begin(), toplevels_.end(),
                                  [&](auto const& candidate) { return candidate.get() == toplevel; }),
                   toplevels_.end());
}

void WaylandServer::destroyShmPool(ShmPool* pool) {
  if (pool->data) munmap(pool->data, static_cast<std::size_t>(pool->size));
  if (pool->fd >= 0) close(pool->fd);
  shmPools_.erase(std::remove_if(shmPools_.begin(), shmPools_.end(),
                                 [&](auto const& candidate) { return candidate.get() == pool; }),
                  shmPools_.end());
}

void WaylandServer::destroyShmBuffer(ShmBuffer* buffer) {
  shmBuffers_.erase(std::remove_if(shmBuffers_.begin(), shmBuffers_.end(),
                                   [&](auto const& candidate) { return candidate.get() == buffer; }),
                    shmBuffers_.end());
}

void WaylandServer::destroyDmabufParams(DmabufParams* params) {
  for (auto& plane : params->planes) {
    if (plane.fd >= 0) close(plane.fd);
    plane.fd = -1;
  }
  dmabufParams_.erase(std::remove_if(dmabufParams_.begin(), dmabufParams_.end(),
                                     [&](auto const& candidate) { return candidate.get() == params; }),
                      dmabufParams_.end());
}

void WaylandServer::destroyDmabufBuffer(DmabufBuffer* buffer) {
  for (auto const& surface : surfaces_) {
    if (surface->dmabufBuffer == buffer) {
      surface->dmabufBuffer = nullptr;
      surface->width = 0;
      surface->height = 0;
      surface->frameWidth = 0;
      surface->frameHeight = 0;
      surface->rgbaPixels.clear();
      ++surface->serial;
    }
  }
  for (auto& plane : buffer->planes) {
    if (plane.fd >= 0) close(plane.fd);
    plane.fd = -1;
  }
  dmabufBuffers_.erase(std::remove_if(dmabufBuffers_.begin(), dmabufBuffers_.end(),
                                      [&](auto const& candidate) { return candidate.get() == buffer; }),
                       dmabufBuffers_.end());
}

void WaylandServer::destroyToplevelDecoration(ToplevelDecoration* decoration) {
  toplevelDecorations_.erase(
      std::remove_if(toplevelDecorations_.begin(), toplevelDecorations_.end(),
                     [&](auto const& candidate) { return candidate.get() == decoration; }),
      toplevelDecorations_.end());
}

} // namespace flux::compositor
