#pragma once

#include <cstdint>

struct wl_client;

namespace lambda::compositor {

class WaylandServer;

void bindPrimarySelectionManager(wl_client* client, void* data, std::uint32_t version, std::uint32_t id);
void bindDataDeviceManager(wl_client* client, void* data, std::uint32_t version, std::uint32_t id);

} // namespace lambda::compositor
