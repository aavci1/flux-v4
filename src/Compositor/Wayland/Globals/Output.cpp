#include "Compositor/Wayland/Globals/Output.hpp"

#include "Compositor/Wayland/WaylandServerImpl.hpp"

#include <algorithm>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

namespace flux::compositor {
namespace {

void outputRelease(wl_client*, wl_resource* resource) {
  wl_resource_destroy(resource);
}

} // namespace

void bindOutput(wl_client* client, void* data, std::uint32_t version, std::uint32_t id) {
  auto* server = static_cast<WaylandServer::Impl*>(data);
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

} // namespace flux::compositor
