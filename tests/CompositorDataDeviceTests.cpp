#include "Compositor/Wayland/DataDeviceDndState.hpp"

#include <doctest/doctest.h>

TEST_CASE("data-device DnD action masks validate protocol bits") {
  using namespace lambda::compositor;

  CHECK(validDndActionMask(WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE));
  CHECK(validDndActionMask(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY));
  CHECK(validDndActionMask(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE));
  CHECK(validDndActionMask(WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK));
  CHECK(validDndActionMask(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                           WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
                           WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK));
  CHECK_FALSE(validDndActionMask(0x80000000u));
  CHECK_FALSE(validDndActionMask(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY | 0x80000000u));

  CHECK(validPreferredDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE));
  CHECK(validPreferredDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY));
  CHECK(validPreferredDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE));
  CHECK(validPreferredDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK));
  CHECK_FALSE(validPreferredDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                                      WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE));
  CHECK_FALSE(validPreferredDndAction(0x80000000u));
}

TEST_CASE("data-device DnD action negotiation honors preference then fallback order") {
  using namespace lambda::compositor;

  std::uint32_t const all = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
  CHECK(chooseDndAction(all, all, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE) ==
        WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
  CHECK(chooseDndAction(all, all, WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK) ==
        WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK);
  CHECK(chooseDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY,
                        WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
                        WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY) ==
        WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE);
  CHECK(chooseDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
                        all,
                        WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE) ==
        WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
  CHECK(chooseDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
                            WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK,
                        all,
                        WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE) ==
        WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
  CHECK(chooseDndAction(WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK,
                        all,
                        WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE) ==
        WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK);
}
