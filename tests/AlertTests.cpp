#include <doctest/doctest.h>

#include "UI/Views/AlertActionHelpers.hpp"

#include <functional>

TEST_CASE("Alert action wrapper preserves the original action after dismiss tears down the owner") {
  bool ranAction = false;
  std::function<void()> wrapped;

  wrapped = flux::detail::wrapDismissThenInvoke(
      [&wrapped] { wrapped = {}; },
      [&ranAction] { ranAction = true; });

  REQUIRE(wrapped);
  wrapped();
  CHECK(ranAction);
}
