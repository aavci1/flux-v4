#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <utility>

namespace flux {

namespace {

std::unordered_set<std::uint64_t> gScrollWheelIdleTimerIds;
std::once_flag gScrollWheelIdleBridgeOnce;

void ensureScrollWheelIdleBridge() {
  std::call_once(gScrollWheelIdleBridgeOnce, [] {
    if (!Application::hasInstance()) {
      return;
    }
    Application::instance().eventQueue().on<TimerEvent>([](TimerEvent const& e) {
      if (gScrollWheelIdleTimerIds.count(e.timerId)) {
        Application::instance().markReactiveDirty();
      }
    });
  });
}

struct ScrollWheelIdleSlot {
  std::uint64_t timerId = 0;
  std::optional<std::chrono::steady_clock::time_point> hideAfter;

  ~ScrollWheelIdleSlot() { cancelTimer(); }

  void cancelTimer() {
    if (timerId != 0) {
      gScrollWheelIdleTimerIds.erase(timerId);
      if (Application::hasInstance()) {
        Application::instance().cancelTimer(timerId);
      }
      timerId = 0;
    }
  }

  void clearHide() {
    cancelTimer();
    hideAfter.reset();
  }

  void scheduleIdleHide(int delayMs) {
    cancelTimer();
    int const ms = std::max(1, delayMs);
    using namespace std::chrono;
    hideAfter = steady_clock::now() + milliseconds(ms);
    if (!Application::hasInstance()) {
      return;
    }
    ensureScrollWheelIdleBridge();
    timerId = Application::instance().scheduleRepeatingTimer(milliseconds(50), 0);
    if (timerId != 0) {
      gScrollWheelIdleTimerIds.insert(timerId);
    }
  }
};

Transition fadeTransition(Theme const& theme, bool fadeIn) {
  if (theme.reducedMotion) {
    return Transition::instant();
  }
  float const d = fadeIn ? theme.durationMedium : theme.durationSlow;
  return Transition::ease(d);
}

void appendScrollIndicators(std::vector<Element>& out, ScrollAxis ax, Point const& off, Size const& viewport,
                            Size const& contentSize, float vOpacity, float hOpacity, Theme const& theme) {
  float const vw = viewport.width;
  float const vh = viewport.height;
  float const cw = contentSize.width;
  float const ch = contentSize.height;
  if (vw <= 0.f || vh <= 0.f) {
    return;
  }

  constexpr float kThick = 5.f;
  constexpr float kMargin = 2.f;
  Color const baseRgb = theme.colorTextMuted;

  if ((ax == ScrollAxis::Vertical || ax == ScrollAxis::Both) && ch > vh + 0.5f && vOpacity > 0.001f) {
    float const maxY = std::max(0.f, ch - vh);
    float thumbH = std::max(24.f, vh * (vh / ch));
    thumbH = std::min(thumbH, std::max(0.f, vh - 2.f * kMargin));
    float const track = std::max(0.f, vh - 2.f * kMargin - thumbH);
    float const t = (maxY > 0.f) ? (off.y / maxY) : 0.f;
    float const thumbY = kMargin + t * track;
    Color c{baseRgb.r, baseRgb.g, baseRgb.b, 0.42f * vOpacity};
    out.emplace_back(Rectangle{}
                         .fill(FillStyle::solid(c))
                         .size(kThick, thumbH)
                         .cornerRadius(kThick * 0.5f)
                         .position(vw - kThick - kMargin, thumbY));
  }

  if ((ax == ScrollAxis::Horizontal || ax == ScrollAxis::Both) && cw > vw + 0.5f && hOpacity > 0.001f) {
    float const maxX = std::max(0.f, cw - vw);
    float thumbW = std::max(24.f, vw * (vw / cw));
    thumbW = std::min(thumbW, std::max(0.f, vw - 2.f * kMargin));
    float const track = std::max(0.f, vw - 2.f * kMargin - thumbW);
    float const t = (maxX > 0.f) ? (off.x / maxX) : 0.f;
    float const thumbX = kMargin + t * track;
    Color c{baseRgb.r, baseRgb.g, baseRgb.b, 0.42f * hOpacity};
    out.emplace_back(Rectangle{}
                         .fill(FillStyle::solid(c))
                         .size(thumbW, kThick)
                         .cornerRadius(kThick * 0.5f)
                         .position(thumbX, vh - kThick - kMargin));
  }
}

} // namespace

Point clampScrollOffset(ScrollAxis axis, Point o, Size const& viewport, Size const& content) {
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

Element ScrollView::body() const {
  auto offset = useState<Point>({0.f, 0.f});
  auto downPoint = useState<Point>({0.f, 0.f});
  auto dragging = useState(false);
  auto viewport = useState<Size>({0.f, 0.f});
  auto content = useState<Size>({0.f, 0.f});
  std::optional<Rect> const layoutRect = useLayoutRect();
  Theme const& theme = useEnvironment<Theme>();
  auto vOpacity = useAnimated<float>(0.f);
  auto hOpacity = useAnimated<float>(0.f);
  ScrollWheelIdleSlot& wheelSlot = StateStore::current()->claimSlot<ScrollWheelIdleSlot>();
  Size effectiveViewport = *viewport;
  if (effectiveViewport.width <= 0.f && layoutRect && layoutRect->width > 0.f) {
    effectiveViewport.width = layoutRect->width;
  }
  if (effectiveViewport.height <= 0.f && layoutRect && layoutRect->height > 0.f) {
    effectiveViewport.height = layoutRect->height;
  }
  ScrollAxis const ax = axis;
  std::vector<Element> contentChildren = children;

  Transition const fadeIn = fadeTransition(theme, true);
  Transition const fadeOut = fadeTransition(theme, false);

  using namespace std::chrono;
  if (wheelSlot.hideAfter && steady_clock::now() >= *wheelSlot.hideAfter && !*dragging) {
    vOpacity.set(0.f, fadeOut);
    hOpacity.set(0.f, fadeOut);
    wheelSlot.clearHide();
  }

  std::vector<Element> zChildren;
  zChildren.reserve(3);
  zChildren.emplace_back(OffsetView{
      .offset = *offset,
      .axis = ax,
      .viewportSize = viewport,
      .contentSize = content,
      .children = std::move(contentChildren),
  });
  appendScrollIndicators(zChildren, ax, *offset, effectiveViewport, *content, *vOpacity, *hOpacity, theme);

  return ZStack{
      .horizontalAlignment = Alignment::Start,
      .verticalAlignment = Alignment::Start,
      .children = std::move(zChildren),
  }
      .clipContent(true)
      .onPointerDown(
          [dragging, offset, downPoint, &wheelSlot](Point p) {
            wheelSlot.clearHide();
            dragging = true;
            downPoint = Point{p.x + (*offset).x, p.y + (*offset).y};
          })
      .onPointerUp(
          [dragging, vOpacity, hOpacity, fadeOut](Point) {
            dragging = false;
            vOpacity.set(0.f, fadeOut);
            hOpacity.set(0.f, fadeOut);
          })
      .onPointerMove(
          [offset, downPoint, ax, content, dragging, effectiveViewport, vOpacity, hOpacity, fadeIn](Point p) {
            if (!*dragging) {
              return;
            }
            Point const prev = *offset;
            Point const next{(*downPoint).x - p.x, (*downPoint).y - p.y};
            Point const clamped = clampScrollOffset(ax, next, effectiveViewport, *content);
            offset = clamped;
            if (clamped.x != prev.x) {
              hOpacity.set(1.f, fadeIn);
            }
            if (clamped.y != prev.y) {
              vOpacity.set(1.f, fadeIn);
            }
          })
      .onScroll(
          [offset, ax, content, effectiveViewport, vOpacity, hOpacity, fadeIn, &wheelSlot](Vec2 d) {
            bool const movedV = (ax == ScrollAxis::Vertical || ax == ScrollAxis::Both) && d.y != 0.f;
            bool const movedH = (ax == ScrollAxis::Horizontal || ax == ScrollAxis::Both) && d.x != 0.f;
            if (movedV) {
              vOpacity.set(1.f, fadeIn);
            }
            if (movedH) {
              hOpacity.set(1.f, fadeIn);
            }
            if (movedV || movedH) {
              wheelSlot.scheduleIdleHide(400);
            }
            Point next = *offset;
            if (ax == ScrollAxis::Vertical || ax == ScrollAxis::Both) {
              next.y -= d.y;
            }
            if (ax == ScrollAxis::Horizontal || ax == ScrollAxis::Both) {
              next.x -= d.x;
            }
            Point const clamped = clampScrollOffset(ax, next, effectiveViewport, *content);
            offset = clamped;
          });
}

} // namespace flux
