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

TEST_CASE("StackLayout: justify center offsets remaining main-axis space") {
  std::array<StackMainAxisChild, 2> children{{
      StackMainAxisChild{.naturalMainSize = 20.f, .minMainSize = 0.f},
      StackMainAxisChild{.naturalMainSize = 10.f, .minMainSize = 0.f},
  }};

  StackMainAxisLayout const layout =
      layoutStackMainAxis(children, 5.f, 50.f, true, JustifyContent::Center);
  CHECK(layout.startOffset == doctest::Approx(7.5f));
  CHECK(layout.itemSpacing == doctest::Approx(5.f));
}

TEST_CASE("StackLayout: justify space-between adds remaining main-axis space to gaps") {
  std::array<StackMainAxisChild, 3> children{{
      StackMainAxisChild{.naturalMainSize = 10.f, .minMainSize = 0.f},
      StackMainAxisChild{.naturalMainSize = 10.f, .minMainSize = 0.f},
      StackMainAxisChild{.naturalMainSize = 10.f, .minMainSize = 0.f},
  }};

  StackMainAxisLayout const layout =
      layoutStackMainAxis(children, 5.f, 100.f, true, JustifyContent::SpaceBetween);
  CHECK(layout.startOffset == doctest::Approx(0.f));
  CHECK(layout.itemSpacing == doctest::Approx(35.f));
}

TEST_CASE("StackLayout: justify space-around falls back to safe center on overflow") {
  std::array<StackMainAxisChild, 2> children{{
      StackMainAxisChild{.naturalMainSize = 20.f, .minMainSize = 0.f},
      StackMainAxisChild{.naturalMainSize = 20.f, .minMainSize = 0.f},
  }};

  StackMainAxisLayout const layout =
      layoutStackMainAxis(children, 5.f, 30.f, true, JustifyContent::SpaceAround);
  CHECK(layout.startOffset == doctest::Approx(0.f));
  CHECK(layout.itemSpacing == doctest::Approx(5.f));
}

TEST_CASE("StackLayout: justify space-evenly centers a single child in remaining space") {
  std::array<StackMainAxisChild, 1> children{{
      StackMainAxisChild{.naturalMainSize = 20.f, .minMainSize = 0.f},
  }};

  StackMainAxisLayout const layout =
      layoutStackMainAxis(children, 8.f, 80.f, true, JustifyContent::SpaceEvenly);
  CHECK(layout.startOffset == doctest::Approx(30.f));
  CHECK(layout.itemSpacing == doctest::Approx(38.f));
}

TEST_CASE("StackLayout: auto flex basis preserves intrinsic size before equal grow") {
  std::array<StackMainAxisChild, 2> children{{
      StackMainAxisChild{.naturalMainSize = 100.f, .minMainSize = 0.f, .flexGrow = 1.f, .flexShrink = 1.f},
      StackMainAxisChild{.naturalMainSize = 200.f, .minMainSize = 0.f, .flexGrow = 1.f, .flexShrink = 1.f},
  }};

  StackMainAxisLayout const layout = layoutStackMainAxis(children, 0.f, 700.f, true);
  REQUIRE(layout.mainSizes.size() == 2);
  CHECK(layout.mainSizes[0] == doctest::Approx(300.f));
  CHECK(layout.mainSizes[1] == doctest::Approx(400.f));
}

TEST_CASE("StackLayout: zero flex basis equalizes equal-grow children") {
  std::array<StackMainAxisChild, 2> children{{
      StackMainAxisChild{
          .naturalMainSize = 100.f,
          .flexBasis = 0.f,
          .minMainSize = 0.f,
          .flexGrow = 1.f,
          .flexShrink = 1.f,
      },
      StackMainAxisChild{
          .naturalMainSize = 200.f,
          .flexBasis = 0.f,
          .minMainSize = 0.f,
          .flexGrow = 1.f,
          .flexShrink = 1.f,
      },
  }};

  StackMainAxisLayout const layout = layoutStackMainAxis(children, 0.f, 700.f, true);
  REQUIRE(layout.mainSizes.size() == 2);
  CHECK(layout.mainSizes[0] == doctest::Approx(350.f));
  CHECK(layout.mainSizes[1] == doctest::Approx(350.f));
}

TEST_CASE("StackLayout: horizontal center alignment uses assigned cross size") {
  std::array<Size, 3> measuredSizes{{
      Size{40.f, 44.f},
      Size{56.f, 88.f},
      Size{32.f, 60.f},
  }};
  std::array<float, 3> mainSizes{{40.f, 56.f, 32.f}};

  StackLayoutResult const layout =
      layoutStack(StackAxis::Horizontal, Alignment::Center, measuredSizes, mainSizes, 8.f, 200.f, 0.f, 152.f, true);

  REQUIRE(layout.slots.size() == 3);
  CHECK(layout.containerSize.height == doctest::Approx(152.f));
  CHECK(layout.slots[0].origin.y == doctest::Approx(54.f));
  CHECK(layout.slots[1].origin.y == doctest::Approx(32.f));
  CHECK(layout.slots[2].origin.y == doctest::Approx(46.f));
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

TEST_CASE("GridLayout: assigned outer height does not flatten per-row intrinsic heights") {
  GridTrackMetrics const metrics =
      resolveGridTrackMetrics(2, 4, 8.f, 6.f, 96.f, true, 100.f, true);
  std::array<Size, 4> childSizes{{
      Size{44.f, 12.f},
      Size{44.f, 12.f},
      Size{44.f, 36.f},
      Size{44.f, 12.f},
  }};

  LayoutConstraints childConstraints{};
  childConstraints.maxWidth = 96.f;
  childConstraints.maxHeight = 100.f;
  childConstraints = gridChildConstraints(childConstraints, metrics);
  CHECK(std::isinf(childConstraints.maxHeight));

  GridLayoutResult const layout =
      layoutGrid(metrics, 8.f, 6.f, 96.f, true, 100.f, true, childSizes);
  REQUIRE(layout.rowHeights.size() == 2);
  CHECK(layout.rowHeights[0] == doctest::Approx(12.f));
  CHECK(layout.rowHeights[1] == doctest::Approx(36.f));
  CHECK(layout.slots[2].y == doctest::Approx(18.f));
  CHECK(layout.slots[2].height == doctest::Approx(36.f));
  CHECK(layout.containerSize.height == doctest::Approx(100.f));
}

TEST_CASE("GridLayout: column spans pack rows and preserve equal-width columns") {
  std::array<std::size_t, 7> spans{{1u, 1u, 1u, 1u, 4u, 2u, 2u}};
  GridTrackMetrics const metrics =
      resolveGridTrackMetrics(4, spans, 8.f, 6.f, 424.f, true, 0.f, false);
  std::array<Size, 7> childSizes{{
      Size{100.f, 12.f},
      Size{100.f, 12.f},
      Size{100.f, 12.f},
      Size{100.f, 12.f},
      Size{424.f, 20.f},
      Size{208.f, 16.f},
      Size{208.f, 18.f},
  }};

  LayoutConstraints constraints{};
  constraints.maxWidth = 424.f;
  LayoutConstraints const spanChildConstraints = gridChildConstraints(constraints, metrics, 4);
  CHECK(spanChildConstraints.maxWidth == doctest::Approx(424.f));

  GridLayoutResult const layout =
      layoutGrid(metrics, 8.f, 6.f, 424.f, true, 0.f, false, childSizes);
  REQUIRE(layout.slots.size() == 7);
  CHECK(layout.slots[4].x == doctest::Approx(0.f));
  CHECK(layout.slots[4].y == doctest::Approx(18.f));
  CHECK(layout.slots[4].width == doctest::Approx(424.f));
  CHECK(layout.slots[5].x == doctest::Approx(0.f));
  CHECK(layout.slots[5].y == doctest::Approx(44.f));
  CHECK(layout.slots[5].width == doctest::Approx(208.f));
  CHECK(layout.slots[6].x == doctest::Approx(216.f));
  CHECK(layout.slots[6].y == doctest::Approx(44.f));
  CHECK(layout.slots[6].width == doctest::Approx(208.f));
  CHECK(layout.containerSize.width == doctest::Approx(424.f));
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
