#include <Flux/UI/Views/Tooltip.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Popover.hpp>
#include <Flux/UI/Views/Text.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_set>

namespace flux {

namespace {

constexpr int kDefaultDelayMs = 600;

std::unordered_set<std::uint64_t> gTooltipTimerIds;
std::once_flag gTooltipTimerBridgeOnce;

struct TooltipTimerSlot {
  std::uint64_t timerId = 0;
  std::optional<std::chrono::steady_clock::time_point> showAfter;

  ~TooltipTimerSlot() { cancel(); }

  void cancel() {
    if (timerId != 0) {
      gTooltipTimerIds.erase(timerId);
      if (Application::hasInstance()) {
        Application::instance().cancelTimer(timerId);
      }
      timerId = 0;
    }
    showAfter.reset();
  }

  void schedule(int delayMs) {
    cancel();
    int const ms = delayMs > 0 ? delayMs : kDefaultDelayMs;
    using namespace std::chrono;
    auto const ns = nanoseconds(static_cast<std::int64_t>(ms) * 1'000'000);
    showAfter = steady_clock::now() + milliseconds(ms);
    timerId = Application::instance().scheduleRepeatingTimer(ns, 0);
    if (timerId != 0) {
      gTooltipTimerIds.insert(timerId);
    }
  }

  bool isActive() const { return timerId != 0; }
};

void ensureTooltipTimerBridge() {
  std::call_once(gTooltipTimerBridgeOnce, [] {
    Application::instance().eventQueue().on<TimerEvent>([](TimerEvent const& e) {
      if (gTooltipTimerIds.count(e.timerId)) {
        // Must schedule a full reactive rebuild so useTooltip runs again; requestRedraw() only
        // repaints the last scene graph and never re-evaluates the delay / showPopover path.
        Application::instance().markReactiveDirty();
      }
    });
  });
}

} // namespace

void useTooltip(TooltipConfig const& config) {
  Theme const& theme = useEnvironment<Theme>();

  bool const hovered = useHover();
  bool const pressed = usePress();
  auto [showPopover, hidePopover, isPresented] = usePopover();

  ensureTooltipTimerBridge();
  TooltipTimerSlot& timer = StateStore::current()->claimSlot<TooltipTimerSlot>();

  int const delayMs = config.delayMs > 0 ? config.delayMs : kDefaultDelayMs;

  if (pressed && isPresented) {
    hidePopover();
    timer.cancel();
    return;
  }

  if (!hovered) {
    timer.cancel();
    if (isPresented) {
      hidePopover();
    }
    return;
  }

  if (isPresented) {
    return;
  }

  if (!timer.isActive()) {
    timer.schedule(delayMs);
    return;
  }

  if (timer.showAfter && std::chrono::steady_clock::now() >= *timer.showAfter) {
    float const maxW = 240.f;
    Popover popover{
        .content = Element{Text{
            .text = config.text,
            .font = theme.fontBodySmall,
            .color = theme.colorTextPrimary,
            .wrapping = TextWrapping::Wrap,
        }},
        .placement = config.placement,
        .gap = theme.space2,
        .arrow = false,
        .backgroundColor = theme.colorSurface,
        .borderColor = theme.colorBorderSubtle,
        .borderWidth = 0.5f,
        .cornerRadius = theme.radiusMedium,
        .contentPadding = theme.space2,
        .maxSize = Size{maxW, 0.f},
        .backdropColor = Colors::transparent,
        .dismissOnEscape = false,
        .dismissOnOutsideTap = false,
        .useTapAnchor = false,
        .useHoverLeafAnchor = true,
    };
    showPopover(std::move(popover));
    timer.cancel();
  }
}

void useTooltip(std::string text) {
  useTooltip(TooltipConfig{.text = std::move(text)});
}

} // namespace flux
