#include "Compositor/WaylandServer.hpp"

#include "linux-dmabuf-unstable-v1-server-protocol.h"
#include "xdg-decoration-unstable-v1-server-protocol.h"
#include "xdg-shell-server-protocol.h"

#include <drm_fourcc.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>
#include <utility>

namespace flux::compositor {
namespace {

WaylandServer* serverFrom(wl_resource* resource) {
  return static_cast<WaylandServer*>(wl_resource_get_user_data(resource));
}

template <typename T>
T* dataFrom(wl_resource* resource) {
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
  std::uint64_t serial = 0;
  std::vector<std::uint8_t> rgbaPixels;
  std::int32_t width = 0;
  std::int32_t height = 0;
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
        ++surface->serial;
      }
    }
    wl_buffer_send_release(surface->currentBuffer);
  } else {
    surface->rgbaPixels.clear();
    surface->width = 0;
    surface->height = 0;
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

void xdgWmBaseCreatePositioner(wl_client* client, wl_resource*, std::uint32_t id) {
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

struct xdg_toplevel_interface const xdgToplevelImpl{
    .destroy = xdgToplevelDestroy,
    .set_parent = [](wl_client*, wl_resource*, wl_resource*) {},
    .set_title = xdgToplevelSetTitle,
    .set_app_id = xdgToplevelSetAppId,
    .show_window_menu = [](wl_client*, wl_resource*, wl_resource*, std::uint32_t, std::int32_t, std::int32_t) {},
    .move = [](wl_client*, wl_resource*, wl_resource*, std::uint32_t) {},
    .resize = [](wl_client*, wl_resource*, wl_resource*, std::uint32_t, std::uint32_t) {},
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

void bindSeat(wl_client* client, void*, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &wl_seat_interface, std::min(version, 7u), id);
  static struct wl_seat_interface const seatImpl{
      [](wl_client*, wl_resource*, std::uint32_t) {},
      [](wl_client*, wl_resource*, std::uint32_t) {},
      [](wl_client*, wl_resource*, std::uint32_t) {},
      outputRelease,
  };
  wl_resource_set_implementation(resource, &seatImpl, nullptr, nullptr);
  wl_seat_send_capabilities(resource, 0);
  if (version >= 2) wl_seat_send_name(resource, "seat0");
}

void bindXdgWmBase(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client, &xdg_wm_base_interface, std::min(version, 6u), id);
  wl_resource_set_implementation(resource, &xdgWmBaseImpl, data, nullptr);
}

void sendDmabufFormat(wl_resource* resource, std::uint32_t format) {
  if (wl_resource_get_version(resource) >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
    std::uint64_t modifier = DRM_FORMAT_MOD_INVALID;
    zwp_linux_dmabuf_v1_send_modifier(resource, format, static_cast<std::uint32_t>(modifier >> 32u),
                                      static_cast<std::uint32_t>(modifier & 0xffffffffu));
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
    if (surface->rgbaPixels.empty() || surface->width <= 0 || surface->height <= 0) continue;
    snapshots.push_back({
        .id = surface->id,
        .x = surface->windowX,
        .y = surface->windowY,
        .width = surface->width,
        .height = surface->height,
        .serial = surface->serial,
        .rgbaPixels = surface->rgbaPixels,
    });
  }
  return snapshots;
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
