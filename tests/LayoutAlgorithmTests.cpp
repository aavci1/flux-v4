#include <doctest/doctest.h>

#include <Flux/UI/Views/Popover.hpp>
#include <Flux/UI/Views/PopoverCalloutShape.hpp>

#include "UI/Layout/Algorithms/GridLayout.hpp"
#include "UI/Layout/Algorithms/OverlayLayout.hpp"
#include "UI/Layout/Algorithms/ScrollLayout.hpp"
#include "UI/Layout/Algorithms/StackLayout.hpp"

#include <cmath>
#include <limits>

namespace {

using namespace flux;
using namespace flux::layout;

TEST_CASE("StackLayout: grow distributes extra space by flex weight") {
  std::array<StackMainAxisChild, 2> children{{
      StackMainAxisChild{.naturalMainSize = 20.f, .minMainSize = 0.f, .flexGrow = 1.f, .flexShrink = 1.f},
      StackMainAxisChild{.naturalMainSize = 10.f, .minMainSize = 0.f, .flexGrow = 3.f, .flexShrink = 1.f},
  }};

  StackMainAxisLayout const layout = layoutStackMainAxis(children, 5.f, 60.f, true);
  REQUIRE(layout.mainSizes.size() == 2);
  CHECK(layout.mainSizes[0] == doctest::Approx(26.25f));
  CHECK(layout.mainSizes[1] == doctest::Approx(28.75f));
  CHECK(layout.containerMainSize == doctest::Approx(60.f));
  CHECK(layout.startOffset == doctest::Approx(0.f));
}

TEST_CASE("StackLayout: shrink clamps at child minimums") {
  std::array<StackMainAxisChild, 2> children{{
      StackMainAxisChild{.naturalMainSize = 40.f, .minMainSize = 35.f, .flexGrow = 0.f, .flexShrink = 1.f},
      StackMainAxisChild{.naturalMainSize = 30.f, .minMainSize = 10.f, .flexGrow = 0.f, .flexShrink = 1.f},
  }};

  StackMainAxisLayout const layout = layoutStackMainAxis(children, 5.f, 55.f, true);
  REQUIRE(layout.mainSizes.size() == 2);
  CHECK(layout.mainSizes[0] == doctest::Approx(35.f));
  CHECK(layout.mainSizes[1] == doctest::Approx(15.f));
  CHECK(layout.containerMainSize == doctest::Approx(55.f));
}

TEST_CASE("GridLayout: intrinsic sizing uses widest column and tallest row") {
  GridTrackMetrics const metrics =
      resolveGridTrackMetrics(2, 3, 8.f, 6.f, 0.f, false, 0.f, false);
  std::array<Size, 3> childSizes{{
      Size{20.f, 10.f},
      Size{40.f, 12.f},
      Size{30.f, 18.f},
  }};

  GridLayoutResult const layout =
      layoutGrid(metrics, 8.f, 6.f, 0.f, false, 0.f, false, childSizes);
  REQUIRE(layout.slots.size() == 3);
  CHECK(layout.containerSize.width == doctest::Approx(78.f));
  CHECK(layout.containerSize.height == doctest::Approx(36.f));
  CHECK(layout.slots[2].x == doctest::Approx(0.f));
  CHECK(layout.slots[2].y == doctest::Approx(18.f));
}

TEST_CASE("ScrollLayout: clamps offsets and assigns vertical child slots") {
  std::array<Size, 2> childSizes{{
      Size{40.f, 60.f},
      Size{40.f, 50.f},
  }};

  ScrollContentLayout const layout =
      layoutScrollContent(ScrollAxis::Vertical, Size{80.f, 70.f}, Point{0.f, 100.f}, childSizes);
  REQUIRE(layout.slots.size() == 2);
  CHECK(layout.contentSize.width == doctest::Approx(40.f));
  CHECK(layout.contentSize.height == doctest::Approx(110.f));
  CHECK(layout.clampedOffset.y == doctest::Approx(40.f));
  CHECK(layout.slots[0].origin.y == doctest::Approx(-40.f));
  CHECK(layout.slots[1].origin.y == doctest::Approx(20.f));
  CHECK(layout.slots[0].assignedSize.width == doctest::Approx(80.f));
}

TEST_CASE("ScrollLayout: indicator metrics track scroll progress") {
  Size const viewport{120.f, 80.f};
  Size const content{120.f, 240.f};
  ScrollIndicatorMetrics const indicator =
      makeVerticalIndicator(Point{0.f, 80.f}, viewport, content, false);

  CHECK(indicator.visible());
  CHECK(indicator.x == doctest::Approx(113.f));
  CHECK(indicator.height > 24.f);
  CHECK(indicator.y > 3.f);
}

TEST_CASE("OverlayLayout: popover callout layout reserves arrow depth in total size") {
  PopoverCalloutShape callout{
      .placement = PopoverPlacement::Below,
      .arrow = true,
      .padding = 12.f,
  };

  PopoverCalloutLayout const layout =
      layoutPopoverCallout(callout, Size{100.f, 20.f}, LayoutConstraints{});
  CHECK(std::isinf(layout.contentConstraints.maxWidth));
  CHECK(layout.totalSize.width == doctest::Approx(124.f));
  CHECK(layout.totalSize.height ==
        doctest::Approx(20.f + 24.f + PopoverCalloutShape::kArrowH));
  CHECK(layout.contentOrigin.y == doctest::Approx(PopoverCalloutShape::kArrowH + 12.f));
}

TEST_CASE("PopoverPlacement: measured popover size avoids premature flip from max-size estimate") {
  Rect const anchor{120.f, 500.f, 80.f, 32.f};
  Size const window{420.f, 640.f};
  std::optional<Rect> anchorOpt{anchor};

  CHECK(resolvePopoverPlacement(PopoverPlacement::Below, anchorOpt, Size{260.f, 220.f}, 14.f, window) ==
        PopoverPlacement::Above);
  CHECK(resolveMeasuredPopoverPlacement(PopoverPlacement::Below, anchorOpt, Size{180.f, 88.f}, 8.f, window) ==
        PopoverPlacement::Below);
}

} // namespace
