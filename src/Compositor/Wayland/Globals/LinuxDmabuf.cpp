#include "Compositor/Wayland/Globals/LinuxDmabuf.hpp"

#include "Compositor/Diagnostics/CrashLog.hpp"
#include "Compositor/Wayland/ResourceTemplates.hpp"
#include "Compositor/Wayland/WaylandServerImpl.hpp"
#include "linux-dmabuf-unstable-v1-server-protocol.h"

#include <drm_fourcc.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <memory>
#include <optional>
#include <vector>

namespace flux::compositor {
namespace {

void bufferDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct wl_buffer_interface const bufferImpl{bufferDestroy};

struct DmabufFormatModifier {
  std::uint32_t format = 0;
  std::uint32_t padding = 0;
  std::uint64_t modifier = 0;
};

constexpr std::array<std::uint32_t, 4> kSupportedDmabufFormats{
    DRM_FORMAT_ARGB8888,
    DRM_FORMAT_XRGB8888,
    DRM_FORMAT_ABGR8888,
    DRM_FORMAT_XBGR8888,
};

constexpr std::array<std::uint64_t, 2> kSupportedDmabufModifiers{
    DRM_FORMAT_MOD_INVALID,
    DRM_FORMAT_MOD_LINEAR,
};

std::optional<DmabufPlane> findPlane(WaylandServer::Impl::DmabufParams const* params, std::uint32_t index) {
  auto found = std::find_if(params->planes.begin(), params->planes.end(),
                            [index](DmabufPlane const& plane) { return plane.index == index; });
  if (found == params->planes.end()) return std::nullopt;
  return *found;
}

std::optional<std::uint32_t> bytesPerPixel(std::uint32_t format) {
  switch (format) {
  case DRM_FORMAT_ARGB8888:
  case DRM_FORMAT_XRGB8888:
  case DRM_FORMAT_ABGR8888:
  case DRM_FORMAT_XBGR8888:
    return 4u;
  default:
    return std::nullopt;
  }
}

bool validateDmabufParams(WaylandServer::Impl::DmabufParams* params, std::int32_t width, std::int32_t height,
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
  if (params->planes.size() != 1) {
    wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                           "only single-plane RGB dmabufs are currently supported");
    return false;
  }
  DmabufPlane const& plane = params->planes.front();
  auto bpp = bytesPerPixel(format);
  if (!bpp || plane.stride < static_cast<std::uint32_t>(width) * *bpp) {
    wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                           "dmabuf plane stride is too small");
    return false;
  }
  if (plane.modifier != DRM_FORMAT_MOD_INVALID && plane.modifier != DRM_FORMAT_MOD_LINEAR) {
    wl_resource_post_error(params->resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_FORMAT,
                           "unsupported dmabuf modifier 0x%016llx",
                           static_cast<unsigned long long>(plane.modifier));
    return false;
  }
  return true;
}

wl_resource* createDmabufBuffer(wl_client* client, WaylandServer::Impl::DmabufParams* params, std::uint32_t id,
                                std::int32_t width, std::int32_t height, std::uint32_t format,
                                std::uint32_t flags) {
  auto buffer = std::make_unique<WaylandServer::Impl::DmabufBuffer>();
  buffer->server = params->server;
  buffer->id = params->server->nextDmabufBufferId_++;
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
  wl_resource_set_implementation(bufferResource,
                                 &bufferImpl,
                                 raw,
                                 destroyResourceCallback<WaylandServer::Impl::DmabufBuffer,
                                                         WaylandServer::Impl,
                                                         &WaylandServer::Impl::destroyDmabufBuffer>);
  diagnostics::crashLog("dmabuf-create id=%llu size=%dx%d format=0x%08x flags=0x%08x stride=%u modifier=0x%016llx",
                        static_cast<unsigned long long>(raw->id),
                        width,
                        height,
                        format,
                        flags,
                        raw->planes.empty() ? 0u : raw->planes.front().stride,
                        static_cast<unsigned long long>(raw->planes.empty() ? 0ull : raw->planes.front().modifier));
  return bufferResource;
}

void linuxBufferParamsDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void linuxBufferParamsAdd(wl_client*, wl_resource* resource, int fd, std::uint32_t planeIndex,
                          std::uint32_t offset, std::uint32_t stride, std::uint32_t modifierHi,
                          std::uint32_t modifierLo) {
  auto* params = resourceData<WaylandServer::Impl::DmabufParams>(resource);
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
  if (stride == 0) {
    close(fd);
    wl_resource_post_error(resource, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_OUT_OF_BOUNDS,
                           "dmabuf plane stride must be positive");
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
  auto* params = resourceData<WaylandServer::Impl::DmabufParams>(resource);
  if (!validateDmabufParams(params, width, height, format)) return;
  params->used = true;
  wl_resource* buffer = createDmabufBuffer(client, params, 0, width, height, format, flags);
  if (buffer) zwp_linux_buffer_params_v1_send_created(resource, buffer);
}

void linuxBufferParamsCreateImmed(wl_client* client, wl_resource* resource, std::uint32_t bufferId,
                                  std::int32_t width, std::int32_t height, std::uint32_t format,
                                  std::uint32_t flags) {
  auto* params = resourceData<WaylandServer::Impl::DmabufParams>(resource);
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
  auto params = std::make_unique<WaylandServer::Impl::DmabufParams>();
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
  wl_resource_set_implementation(paramsResource,
                                 &linuxBufferParamsImpl,
                                 raw,
                                 destroyResourceCallback<WaylandServer::Impl::DmabufParams,
                                                         WaylandServer::Impl,
                                                         &WaylandServer::Impl::destroyDmabufParams>);
}

void linuxDmabufFeedbackDestroy(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

struct zwp_linux_dmabuf_feedback_v1_interface const linuxDmabufFeedbackImpl{
    .destroy = linuxDmabufFeedbackDestroy,
};

std::vector<DmabufFormatModifier> supportedDmabufFormatTable() {
  std::vector<DmabufFormatModifier> table;
  table.reserve(kSupportedDmabufFormats.size() * kSupportedDmabufModifiers.size());
  for (std::uint32_t format : kSupportedDmabufFormats) {
    for (std::uint64_t modifier : kSupportedDmabufModifiers) {
      table.push_back({
          .format = format,
          .padding = 0,
          .modifier = modifier,
      });
    }
  }
  return table;
}

int createDmabufFormatTableFd(std::vector<DmabufFormatModifier> const& table) {
  int fd = memfd_create("lambda-dmabuf-formats", MFD_CLOEXEC);
  if (fd < 0) return -1;

  std::size_t const byteSize = table.size() * sizeof(DmabufFormatModifier);
  auto const* data = reinterpret_cast<char const*>(table.data());
  std::size_t written = 0;
  while (written < byteSize) {
    ssize_t const rc = write(fd, data + written, byteSize - written);
    if (rc < 0) {
      if (errno == EINTR) continue;
      close(fd);
      return -1;
    }
    if (rc == 0) {
      close(fd);
      return -1;
    }
    written += static_cast<std::size_t>(rc);
  }
  return fd;
}

void appendArrayBytes(wl_array& array, void const* data, std::size_t size) {
  void* target = wl_array_add(&array, size);
  if (!target) return;
  std::memcpy(target, data, size);
}

void sendDeviceArray(wl_resource* resource, bool trancheTarget, std::uint64_t rawDevice) {
  wl_array deviceArray;
  wl_array_init(&deviceArray);
  dev_t const device = static_cast<dev_t>(rawDevice);
  appendArrayBytes(deviceArray, &device, sizeof(device));
  if (deviceArray.size == sizeof(device)) {
    if (trancheTarget) {
      zwp_linux_dmabuf_feedback_v1_send_tranche_target_device(resource, &deviceArray);
    } else {
      zwp_linux_dmabuf_feedback_v1_send_main_device(resource, &deviceArray);
    }
  }
  wl_array_release(&deviceArray);
}

void sendDmabufFeedback(wl_resource* resource, WaylandServer::Impl* server) {
  auto const table = supportedDmabufFormatTable();
  int tableFd = createDmabufFormatTableFd(table);
  if (tableFd < 0) {
    wl_resource_post_no_memory(resource);
    return;
  }

  zwp_linux_dmabuf_feedback_v1_send_format_table(
      resource, tableFd, static_cast<std::uint32_t>(table.size() * sizeof(DmabufFormatModifier)));
  close(tableFd);

  std::uint64_t const device = server && server->output_.drmDevice != 0 ? server->output_.drmDevice : 1;
  sendDeviceArray(resource, false, device);
  sendDeviceArray(resource, true, device);

  wl_array indices;
  wl_array_init(&indices);
  for (std::uint16_t i = 0; i < table.size(); ++i) {
    appendArrayBytes(indices, &i, sizeof(i));
  }
  if (indices.size == table.size() * sizeof(std::uint16_t)) {
    zwp_linux_dmabuf_feedback_v1_send_tranche_formats(resource, &indices);
  }
  wl_array_release(&indices);

  zwp_linux_dmabuf_feedback_v1_send_tranche_flags(resource, 0);
  zwp_linux_dmabuf_feedback_v1_send_tranche_done(resource);
  zwp_linux_dmabuf_feedback_v1_send_done(resource);
}

wl_resource* createDmabufFeedbackResource(wl_client* client, wl_resource* dmabufResource, std::uint32_t id) {
  wl_resource* resource = wl_resource_create(client,
                                             &zwp_linux_dmabuf_feedback_v1_interface,
                                             wl_resource_get_version(dmabufResource),
                                             id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return nullptr;
  }
  wl_resource_set_implementation(resource, &linuxDmabufFeedbackImpl, nullptr, nullptr);
  return resource;
}

void linuxDmabufGetDefaultFeedback(wl_client* client, wl_resource* resource, std::uint32_t id) {
  wl_resource* feedback = createDmabufFeedbackResource(client, resource, id);
  if (!feedback) return;
  sendDmabufFeedback(feedback, serverFrom(resource));
}

void linuxDmabufGetSurfaceFeedback(wl_client* client, wl_resource* resource, std::uint32_t id,
                                   wl_resource* surfaceResource) {
  (void)surfaceResource;
  wl_resource* feedback = createDmabufFeedbackResource(client, resource, id);
  if (!feedback) return;
  sendDmabufFeedback(feedback, serverFrom(resource));
}

struct zwp_linux_dmabuf_v1_interface const linuxDmabufImpl{
    .destroy = linuxDmabufDestroy,
    .create_params = linuxDmabufCreateParams,
    .get_default_feedback = linuxDmabufGetDefaultFeedback,
    .get_surface_feedback = linuxDmabufGetSurfaceFeedback,
};

void sendDmabufFormat(wl_resource* resource, std::uint32_t format) {
  if (wl_resource_get_version(resource) >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
    for (std::uint64_t modifier : {DRM_FORMAT_MOD_INVALID, DRM_FORMAT_MOD_LINEAR}) {
      zwp_linux_dmabuf_v1_send_modifier(resource,
                                        format,
                                        static_cast<std::uint32_t>(modifier >> 32u),
                                        static_cast<std::uint32_t>(modifier & 0xffffffffu));
    }
    return;
  }
  zwp_linux_dmabuf_v1_send_format(resource, format);
}

} // namespace

bool isSupportedDmabufFormat(std::uint32_t format) {
  return std::find(kSupportedDmabufFormats.begin(), kSupportedDmabufFormats.end(), format) !=
         kSupportedDmabufFormats.end();
}

void bindLinuxDmabuf(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  std::uint32_t const resourceVersion = std::min(version, 5u);
  wl_resource* resource = wl_resource_create(client, &zwp_linux_dmabuf_v1_interface, resourceVersion, id);
  if (!resource) {
    wl_client_post_no_memory(client);
    return;
  }
  wl_resource_set_implementation(resource, &linuxDmabufImpl, data, nullptr);
  if (resourceVersion < 4u) {
    for (std::uint32_t format : kSupportedDmabufFormats) {
      sendDmabufFormat(resource, format);
    }
  }
}

} // namespace flux::compositor
