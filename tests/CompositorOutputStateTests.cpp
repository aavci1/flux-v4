#include "Compositor/Wayland/OutputState.hpp"

#include <doctest/doctest.h>

namespace lambda::compositor {

TEST_CASE("output resources use the implemented protocol version") {
  CHECK(kOutputVersion == 4);
  CHECK(outputResourceVersion(1) == 1);
  CHECK(outputResourceVersion(4) == 4);
  CHECK(outputResourceVersion(5) == 4);
}

TEST_CASE("legacy output scale rounds fractional scales up") {
  CHECK(outputIntegerScale(0.5f) == 1);
  CHECK(outputIntegerScale(1.0f) == 1);
  CHECK(outputIntegerScale(1.25f) == 2);
  CHECK(outputIntegerScale(1.5f) == 2);
  CHECK(outputIntegerScale(2.0f) == 2);
  CHECK(outputIntegerScale(2.25f) == 3);
  CHECK(outputIntegerScale(4.0f) == 4);
}

} // namespace lambda::compositor
