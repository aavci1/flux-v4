#include <doctest/doctest.h>

namespace {

bool containsPoint(float x, float y, float left, float top, float right, float bottom) {
  return x >= left && x < right && y >= top && y < bottom;
}

bool subsurfaceTreeContains(float x,
                            float y,
                            float parentX,
                            float parentY,
                            float subsurfaceX,
                            float subsurfaceY,
                            float subsurfaceWidth,
                            float subsurfaceHeight) {
  float const left = parentX + subsurfaceX;
  float const top = parentY + subsurfaceY;
  return containsPoint(x, y, left, top, left + subsurfaceWidth, top + subsurfaceHeight);
}

} // namespace

TEST_CASE("subsurface hit testing uses parent offset and child size") {
  CHECK(subsurfaceTreeContains(130.f, 80.f, 100.f, 50.f, 10.f, 20.f, 40.f, 30.f));
  CHECK_FALSE(subsurfaceTreeContains(105.f, 80.f, 100.f, 50.f, 10.f, 20.f, 40.f, 30.f));
  CHECK_FALSE(subsurfaceTreeContains(130.f, 65.f, 100.f, 50.f, 10.f, 20.f, 40.f, 30.f));
}

TEST_CASE("nested subsurface coordinates accumulate") {
  float const nestedLeft = 10.f + 5.f;
  float const nestedTop = 10.f + 5.f;
  CHECK(containsPoint(21.f, 21.f, nestedLeft, nestedTop, nestedLeft + 20.f, nestedTop + 20.f));
}
