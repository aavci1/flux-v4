#pragma once

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/OffsetView.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace flux {

inline Point clampScrollOffset(ScrollAxis axis, Point o, Size const& viewport, Size const& content) {
  float maxX = 0.f;
  float maxY = 0.f;
  if (axis == ScrollAxis::Vertical || axis == ScrollAxis::Both) {
    maxY = std::max(0.f, content.height - viewport.height);
  }
  if (axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both) {
    maxX = std::max(0.f, content.width - viewport.width);
  }
  Point r = o;
  if (axis == ScrollAxis::Vertical || axis == ScrollAxis::Both) {
    r.y = std::clamp(r.y, 0.f, maxY);
  } else {
    r.y = 0.f;
  }
  if (axis == ScrollAxis::Horizontal || axis == ScrollAxis::Both) {
    r.x = std::clamp(r.x, 0.f, maxX);
  } else {
    r.x = 0.f;
  }
  return r;
}

struct ScrollView {
  ScrollAxis axis = ScrollAxis::Vertical;
  float flexGrow = 1.f;
  float flexShrink = 0.f;
  float minSize = 0.f;
  std::vector<Element> children;

  auto body() const {
    auto offset = useState<Point>({0.f, 0.f});
    auto downPoint = useState<Point>({0.f, 0.f});
    auto dragging = useState(false);
    auto viewport = useState<Size>({0.f, 0.f});
    auto content = useState<Size>({0.f, 0.f});
    ScrollAxis const ax = axis;
    std::vector<Element> contentChildren = children;

    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .clip = true,
        .children =
            {
                OffsetView{
                    .offset = *offset,
                    .axis = ax,
                    .viewportSize = viewport,
                    .contentSize = content,
                    .children = std::move(contentChildren),
                },
                Rectangle{
                    .fill = FillStyle::none(),
                    .stroke = StrokeStyle::none(),
                    .flexGrow = 1.f,
                    .onPointerDown =
                        [dragging, offset, downPoint](Point p) {
                          dragging = true;
                          downPoint = Point{p.x + (*offset).x, p.y + (*offset).y};
                        },
                    .onPointerUp =
                        [dragging](Point) {
                          dragging = false;
                        },
                    .onPointerMove =
                        [offset, downPoint, ax, viewport, content, dragging](Point p) {
                          if (!*dragging) {
                            return;
                          }
                          Point const next{(*downPoint).x - p.x, (*downPoint).y - p.y};
                          offset = clampScrollOffset(ax, next, *viewport, *content);
                        },
                    .onScroll =
                        [offset, ax, viewport, content](Vec2 d) {
                          Point next = *offset;
                          // scrollingDelta* is expressed for non-flipped NSView coords (y up). Flux uses
                          // a flipped space (y down), so negate to align. Natural Scrolling is already in
                          // the delta sign from AppKit; this is only the coordinate-system fix.
                          if (ax == ScrollAxis::Vertical || ax == ScrollAxis::Both) {
                            next.y -= d.y;
                          }
                          if (ax == ScrollAxis::Horizontal || ax == ScrollAxis::Both) {
                            next.x -= d.x;
                          }
                          offset = clampScrollOffset(ax, next, *viewport, *content);
                        },
                },
            },
    };
  }
};

} // namespace flux
