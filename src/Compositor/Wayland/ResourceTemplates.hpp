#pragma once

#include <wayland-server-core.h>

namespace flux::compositor {

template <typename T>
T* resourceData(wl_resource* resource) {
  return static_cast<T*>(wl_resource_get_user_data(resource));
}

template <typename T, typename Owner, void (Owner::*Destroy)(T*)>
void destroyResourceCallback(wl_resource* resource) {
  if (auto* object = resourceData<T>(resource)) {
    (object->server->*Destroy)(object);
  }
}

} // namespace flux::compositor
