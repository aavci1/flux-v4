#include <Flux/UI/Views/Toast.hpp>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/UI/Overlay.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Card.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <unordered_set>
#include <utility>

namespace flux {

namespace {

using ToastClock = std::chrono::steady_clock;

struct ToastEntry {
  Toast toast;
  std::optional<ToastClock::time_point> dismissAt;
};

bool operator==(ToastEntry const& lhs, ToastEntry const& rhs) {
  bool const sameAction =
      lhs.toast.action.has_value() == rhs.toast.action.has_value() &&
      (!lhs.toast.action.has_value() ||
       (lhs.toast.action->label == rhs.toast.action->label &&
        lhs.toast.action->variant == rhs.toast.action->variant &&
        lhs.toast.action->dismissOnTap == rhs.toast.action->dismissOnTap));

  bool const sameIcon =
      lhs.toast.icon.has_value() == rhs.toast.icon.has_value() &&
      (!lhs.toast.icon.has_value() || *lhs.toast.icon == *rhs.toast.icon);

  return lhs.toast.id == rhs.toast.id && lhs.toast.title == rhs.toast.title &&
         lhs.toast.message == rhs.toast.message && lhs.toast.tone == rhs.toast.tone &&
         lhs.toast.placement == rhs.toast.placement && sameIcon && sameAction &&
         lhs.toast.showCloseButton == rhs.toast.showCloseButton &&
         lhs.toast.autoDismissMs == rhs.toast.autoDismissMs && lhs.toast.minWidth == rhs.toast.minWidth &&
         lhs.toast.maxWidth == rhs.toast.maxWidth && lhs.dismissAt == rhs.dismissAt;
}

struct ToastPalette {
  Color accent;
  Color accentForeground;
  Color surface;
  Color border;
};

std::unordered_set<std::uint64_t> gToastTimerIds;
std::once_flag gToastTimerBridgeOnce;

struct ToastTimerSlot {
  std::uint64_t timerId = 0;
  std::optional<ToastClock::time_point> deadline;

  ~ToastTimerSlot() { cancel(); }

  void cancel() {
    if (timerId != 0) {
      gToastTimerIds.erase(timerId);
      if (Application::hasInstance()) {
        Application::instance().cancelTimer(timerId);
      }
      timerId = 0;
    }
    deadline.reset();
  }

  void schedule(ToastClock::time_point when) {
    using namespace std::chrono;

    auto const now = ToastClock::now();
    auto interval = duration_cast<nanoseconds>(when > now ? when - now : milliseconds(1));
    if (interval <= nanoseconds::zero()) {
      interval = milliseconds(1);
    }

    if (deadline && timerId != 0) {
      auto const current = duration_cast<milliseconds>(*deadline - now).count();
      auto const next = duration_cast<milliseconds>(when - now).count();
      if (current == next) {
        return;
      }
    }

    cancel();
    deadline = when;
    timerId = Application::instance().scheduleRepeatingTimer(interval, 0);
    if (timerId != 0) {
      gToastTimerIds.insert(timerId);
    }
  }
};

struct ToastOverlaySyncSlot {
  std::size_t hash = 0;
  bool visible = false;
};

Color withAlpha(Color c, float alpha) {
  c.a = alpha;
  return c;
}

ToastPalette paletteFor(ToastTone tone, Theme const& theme) {
  switch (tone) {
    case ToastTone::Accent:
      return {
          .accent = theme.accentColor,
          .accentForeground = theme.accentForegroundColor,
          .surface = theme.elevatedBackgroundColor,
          .border = withAlpha(theme.accentColor, 0.24f),
      };
    case ToastTone::Success:
      return {
          .accent = theme.successColor,
          .accentForeground = theme.successForegroundColor,
          .surface = theme.elevatedBackgroundColor,
          .border = withAlpha(theme.successColor, 0.24f),
      };
    case ToastTone::Warning:
      return {
          .accent = theme.warningColor,
          .accentForeground = theme.warningForegroundColor,
          .surface = theme.elevatedBackgroundColor,
          .border = withAlpha(theme.warningColor, 0.26f),
      };
    case ToastTone::Danger:
      return {
          .accent = theme.dangerColor,
          .accentForeground = theme.dangerForegroundColor,
          .surface = theme.elevatedBackgroundColor,
          .border = withAlpha(theme.dangerColor, 0.24f),
      };
    case ToastTone::Neutral:
    default:
      return {
          .accent = theme.labelColor,
          .accentForeground = Colors::white,
          .surface = theme.elevatedBackgroundColor,
          .border = theme.separatorColor,
      };
  }
}

IconName defaultIconFor(ToastTone tone) {
  switch (tone) {
    case ToastTone::Accent:
      return IconName::Info;
    case ToastTone::Success:
      return IconName::CheckCircle;
    case ToastTone::Warning:
      return IconName::Warning;
    case ToastTone::Danger:
      return IconName::Error;
    case ToastTone::Neutral:
    default:
      return IconName::Toast;
  }
}

Alignment alignmentFor(ToastPlacement placement) {
  switch (placement) {
    case ToastPlacement::TopLeading:
    case ToastPlacement::BottomLeading:
      return Alignment::Start;
    case ToastPlacement::TopTrailing:
    case ToastPlacement::BottomTrailing:
      return Alignment::End;
    case ToastPlacement::TopCenter:
    case ToastPlacement::BottomCenter:
    default:
      return Alignment::Center;
  }
}

bool isTopPlacement(ToastPlacement placement) {
  switch (placement) {
    case ToastPlacement::TopLeading:
    case ToastPlacement::TopCenter:
    case ToastPlacement::TopTrailing:
      return true;
    case ToastPlacement::BottomLeading:
    case ToastPlacement::BottomCenter:
    case ToastPlacement::BottomTrailing:
    default:
      return false;
  }
}

float toastStackSpacing(Theme const& theme) {
  float const shadowRadius = std::max(theme.shadowRadiusPopover, 10.f);
  float const shadowOffset = std::max(theme.shadowOffsetYPopover, 3.f);
  return theme.space4 + shadowOffset + shadowRadius * 0.4f;
}

float resolveToastWidth(Toast const& toast, float availableWidth) {
  if (availableWidth <= 0.f) {
    return 0.f;
  }

  float const maxWidth = toast.maxWidth > 0.f ? toast.maxWidth : availableWidth;
  float const minWidth = toast.minWidth > 0.f ? toast.minWidth : 0.f;
  float width = std::min(maxWidth, availableWidth);
  width = std::max(width, std::min(minWidth, availableWidth));
  return width;
}

std::size_t hashToast(Toast const& toast) {
  std::size_t seed = std::hash<std::uint64_t>{}(toast.id);
  seed = detail::combineHash(seed, std::hash<std::string>{}(toast.title));
  seed = detail::combineHash(seed, std::hash<std::string>{}(toast.message));
  seed = detail::combineHash(seed, std::hash<int>{}(static_cast<int>(toast.tone)));
  seed = detail::combineHash(seed, std::hash<int>{}(static_cast<int>(toast.placement)));
  seed = detail::combineHash(seed, std::hash<bool>{}(toast.showCloseButton));
  seed = detail::combineHash(seed, std::hash<int>{}(toast.autoDismissMs));
  seed = detail::combineHash(seed, std::hash<float>{}(toast.minWidth));
  seed = detail::combineHash(seed, std::hash<float>{}(toast.maxWidth));
  if (toast.icon.has_value()) {
    seed = detail::combineHash(seed, std::hash<int>{}(static_cast<int>(*toast.icon)));
  }
  if (toast.action.has_value()) {
    seed = detail::combineHash(seed, std::hash<std::string>{}(toast.action->label));
    seed = detail::combineHash(seed, std::hash<int>{}(static_cast<int>(toast.action->variant)));
    seed = detail::combineHash(seed, std::hash<bool>{}(toast.action->dismissOnTap));
  }
  return seed;
}

std::size_t hashToastList(std::vector<Toast> const& toasts) {
  std::size_t seed = 0;
  for (Toast const& toast : toasts) {
    seed = detail::combineHash(seed, hashToast(toast));
  }
  return seed;
}

std::vector<Toast> publicToasts(std::vector<ToastEntry> const& entries) {
  std::vector<Toast> toasts;
  toasts.reserve(entries.size());
  for (ToastEntry const& entry : entries) {
    toasts.push_back(entry.toast);
  }
  return toasts;
}

Element buildAction(Toast const& toast, Theme const& theme, Color accent,
                    std::function<void(std::uint64_t)> const& onDismiss) {
  if (!toast.action.has_value()) {
    return Rectangle {}.size(0.f, 0.f);
  }

  ToastAction const& action = *toast.action;
  std::string const label = action.label;
  ButtonVariant const variant = action.variant;
  bool const dismissOnTap = action.dismissOnTap;
  std::function<void()> callback = action.action;

  auto handleTap = [toastId = toast.id, dismissOnTap, callback = std::move(callback), onDismiss] {
    if (dismissOnTap && onDismiss) {
      onDismiss(toastId);
    }
    if (callback) {
      callback();
    }
  };

  if (label.empty()) {
    return Rectangle {}.size(0.f, 0.f);
  }

  if (variant == ButtonVariant::Ghost) {
    return LinkButton {
        .label = label,
        .style = LinkButton::Style {
            .font = Font::footnote(),
            .color = accent,
        },
        .onTap = std::move(handleTap),
    };
  }

  return Button {
      .label = label,
      .variant = variant,
      .style = Button::Style {
          .font = Font::footnote(),
          .paddingH = theme.space2,
          .paddingV = theme.space1,
          .cornerRadius = theme.radiusSmall,
          .accentColor = accent,
      },
      .onTap = std::move(handleTap),
  };
}

Element buildToastCard(Toast const& toast, float availableWidth,
                       std::function<void(std::uint64_t)> const& onDismiss) {
  Theme const& theme = useEnvironment<Theme>();
  ToastPalette const palette = paletteFor(toast.tone, theme);
  float const width = resolveToastWidth(toast, availableWidth);
  IconName const icon = toast.icon.value_or(defaultIconFor(toast.tone));
  ShadowStyle const shadow = ShadowStyle {
      .radius = std::max(theme.shadowRadiusPopover, 10.f),
      .offset = {0.f, std::max(theme.shadowOffsetYPopover, 3.f)},
      .color = withAlpha(theme.shadowColor, std::min(theme.shadowColor.a + 0.08f, 0.24f)),
  };

  std::vector<Element> textChildren;
  if (!toast.title.empty()) {
    textChildren.push_back(Text {
        .text = toast.title,
        .font = Font::headline(),
        .color = theme.labelColor,
        .horizontalAlignment = HorizontalAlignment::Leading,
        .wrapping = TextWrapping::Wrap,
    });
  }
  if (!toast.message.empty()) {
    textChildren.push_back(Text {
        .text = toast.message,
        .font = toast.title.empty() ? Font::body() : Font::subheadline(),
        .color = theme.secondaryLabelColor,
        .horizontalAlignment = HorizontalAlignment::Leading,
        .wrapping = TextWrapping::Wrap,
    });
  }
  if (toast.action.has_value()) {
    textChildren.push_back(buildAction(toast, theme, palette.accent, onDismiss));
  }

  Element iconBadge = ZStack {
      .horizontalAlignment = Alignment::Center,
      .verticalAlignment = Alignment::Center,
      .children = children(
          Rectangle {}
              .size(28.f, 28.f)
              .fill(FillStyle::solid(withAlpha(palette.accent, toast.tone == ToastTone::Neutral ? 0.10f : 0.14f)))
              .cornerRadius(CornerRadius {theme.radiusLarge}),
          Icon {
              .name = icon,
              .size = 16.f,
              .weight = 600.f,
              .color = palette.accent,
          })
  }
      .size(28.f, 28.f);

  std::vector<Element> rowChildren;
  rowChildren.push_back(std::move(iconBadge));
  rowChildren.push_back(VStack {
                            .spacing = theme.space1,
                            .alignment = Alignment::Start,
                            .children = std::move(textChildren),
                        }
                            .flex(1.f, 1.f, 0.f));

  if (toast.showCloseButton) {
    rowChildren.push_back(IconButton {
        .icon = IconName::Close,
        .style = IconButton::Style {
            .size = 16.f,
            .weight = 500.f,
            .color = theme.tertiaryLabelColor,
        },
        .onTap = [toastId = toast.id, onDismiss] {
          if (onDismiss) {
            onDismiss(toastId);
          }
        },
    });
  }

  return Card {
      .child = HStack {
          .spacing = theme.space3,
          .alignment = Alignment::Start,
          .children = std::move(rowChildren),
      },
      .style = Card::Style {
          .paddingH = theme.space4,
          .paddingV = theme.space3,
          .cornerRadius = theme.radiusXLarge,
          .borderWidth = 1.f,
          .backgroundColor = palette.surface,
          .borderColor = palette.border,
          .shadow = shadow,
      },
  }
      .cursor(Cursor::Arrow)
      .onPointerDown([](Point) {})
      .width(width);
}

Element buildPlacementLayer(std::vector<Toast> const& toasts, ToastPlacement placement, float width, float height,
                            std::function<void(std::uint64_t)> const& onDismiss) {
  if (toasts.empty()) {
    return Rectangle {}.size(0.f, 0.f);
  }

  Theme const& theme = useEnvironment<Theme>();
  float const availableWidth = std::max(0.f, width - 2.f * theme.space6);
  float const stackSpacing = toastStackSpacing(theme);

  std::vector<Element> toastChildren;
  toastChildren.reserve(toasts.size());
  for (Toast const& toast : toasts) {
    toastChildren.push_back(buildToastCard(toast, availableWidth, onDismiss));
  }

  Element stack = VStack {
      .spacing = stackSpacing,
      .alignment = alignmentFor(placement),
      .children = std::move(toastChildren),
  };

  std::vector<Element> containerChildren;
  if (isTopPlacement(placement)) {
    containerChildren.push_back(std::move(stack));
    containerChildren.push_back(Spacer {});
  } else {
    containerChildren.push_back(Spacer {});
    containerChildren.push_back(std::move(stack));
  }

  return VStack {
      .spacing = 0.f,
      .alignment = alignmentFor(placement),
      .children = std::move(containerChildren),
  }
      .size(width, height)
      .padding(theme.space6);
}

void ensureToastTimerBridge() {
  std::call_once(gToastTimerBridgeOnce, [] {
    Application::instance().eventQueue().on<TimerEvent>([](TimerEvent const& event) {
      if (gToastTimerIds.count(event.timerId)) {
        Application::instance().markReactiveDirty();
      }
    });
  });
}

} // namespace

Element ToastOverlay::body() const {
  if (toasts.empty()) {
    return Rectangle {}.size(0.f, 0.f);
  }

  LayoutConstraints const* constraints = useLayoutConstraints();
  float const width = constraints && std::isfinite(constraints->maxWidth) ? std::max(0.f, constraints->maxWidth) : 0.f;
  float const height =
      constraints && std::isfinite(constraints->maxHeight) ? std::max(0.f, constraints->maxHeight) : 0.f;

  std::vector<Toast> topLeading;
  std::vector<Toast> topCenter;
  std::vector<Toast> topTrailing;
  std::vector<Toast> bottomLeading;
  std::vector<Toast> bottomCenter;
  std::vector<Toast> bottomTrailing;

  for (Toast const& toast : toasts) {
    switch (toast.placement) {
      case ToastPlacement::TopLeading:
        topLeading.push_back(toast);
        break;
      case ToastPlacement::TopCenter:
        topCenter.push_back(toast);
        break;
      case ToastPlacement::TopTrailing:
        topTrailing.push_back(toast);
        break;
      case ToastPlacement::BottomLeading:
        bottomLeading.push_back(toast);
        break;
      case ToastPlacement::BottomCenter:
        bottomCenter.push_back(toast);
        break;
      case ToastPlacement::BottomTrailing:
        bottomTrailing.push_back(toast);
        break;
    }
  }

  return ZStack {
      .horizontalAlignment = Alignment::Start,
      .verticalAlignment = Alignment::Start,
      .children = children(
          buildPlacementLayer(topLeading, ToastPlacement::TopLeading, width, height, onDismiss),
          buildPlacementLayer(topCenter, ToastPlacement::TopCenter, width, height, onDismiss),
          buildPlacementLayer(topTrailing, ToastPlacement::TopTrailing, width, height, onDismiss),
          buildPlacementLayer(bottomLeading, ToastPlacement::BottomLeading, width, height, onDismiss),
          buildPlacementLayer(bottomCenter, ToastPlacement::BottomCenter, width, height, onDismiss),
          buildPlacementLayer(bottomTrailing, ToastPlacement::BottomTrailing, width, height, onDismiss))
  }
      .size(width, height);
}

std::tuple<std::function<std::uint64_t(Toast)>, std::function<void(std::uint64_t)>, std::function<void()>, bool>
useToast() {
  auto [showOverlay, hideOverlay, isPresented] = useOverlay();
  StateStore* store = StateStore::current();
  assert(store && "useToast must be called inside body()");

  auto entries = useState(std::vector<ToastEntry> {});
  auto nextId = useState<std::uint64_t>(1);
  auto revision = useState<std::uint64_t>(0);

  ensureToastTimerBridge();
  ToastTimerSlot& timer = store->claimSlot<ToastTimerSlot>();
  ToastOverlaySyncSlot& sync = store->claimSlot<ToastOverlaySyncSlot>();

  auto dismiss = [entries, revision](std::uint64_t toastId) {
    if (toastId == 0) {
      return;
    }

    std::vector<ToastEntry> next;
    next.reserve((*entries).size());
    std::function<void()> callback;
    for (ToastEntry const& entry : *entries) {
      if (entry.toast.id == toastId) {
        callback = entry.toast.onDismiss;
        continue;
      }
      next.push_back(entry);
    }
    if (next.size() == (*entries).size()) {
      return;
    }
    entries = std::move(next);
    revision = *revision + 1;
    if (callback) {
      callback();
    }
  };

  auto clear = [entries, revision] {
    if ((*entries).empty()) {
      return;
    }
    std::vector<std::function<void()>> callbacks;
    callbacks.reserve((*entries).size());
    for (ToastEntry const& entry : *entries) {
      if (entry.toast.onDismiss) {
        callbacks.push_back(entry.toast.onDismiss);
      }
    }
    entries = std::vector<ToastEntry> {};
    revision = *revision + 1;
    for (std::function<void()> const& callback : callbacks) {
      if (callback) {
        callback();
      }
    }
  };

  auto show = [entries, nextId, revision](Toast toast) -> std::uint64_t {
    if (toast.title.empty() && toast.message.empty()) {
      return 0;
    }

    std::uint64_t const assignedId = toast.id != 0 ? toast.id : *nextId;
    toast.id = assignedId;
    if (toast.id == *nextId) {
      nextId = *nextId + 1;
    } else if (toast.id > *nextId) {
      nextId = toast.id + 1;
    }

    ToastEntry entry;
    entry.toast = std::move(toast);
    if (entry.toast.autoDismissMs > 0) {
      entry.dismissAt = ToastClock::now() + std::chrono::milliseconds(entry.toast.autoDismissMs);
    }

    std::vector<ToastEntry> next = *entries;
    bool replaced = false;
    for (ToastEntry& current : next) {
      if (current.toast.id == assignedId) {
        current = std::move(entry);
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      next.push_back(std::move(entry));
    }
    entries = std::move(next);
    revision = *revision + 1;
    return assignedId;
  };

  std::vector<ToastEntry> activeEntries = *entries;
  std::vector<std::function<void()>> expiredCallbacks;
  auto const now = ToastClock::now();
  std::erase_if(activeEntries, [&](ToastEntry const& entry) {
    bool const expired = entry.dismissAt && now >= *entry.dismissAt;
    if (expired && entry.toast.onDismiss) {
      expiredCallbacks.push_back(entry.toast.onDismiss);
    }
    return expired;
  });
  if (activeEntries.size() != (*entries).size()) {
    entries.setSilently(activeEntries);
    revision.setSilently(*revision + 1);
    for (std::function<void()> const& callback : expiredCallbacks) {
      if (callback) {
        callback();
      }
    }
  }

  std::optional<ToastClock::time_point> nextDeadline;
  for (ToastEntry const& entry : activeEntries) {
    if (!entry.dismissAt.has_value()) {
      continue;
    }
    if (!nextDeadline || *entry.dismissAt < *nextDeadline) {
      nextDeadline = entry.dismissAt;
    }
  }

  if (nextDeadline.has_value()) {
    timer.schedule(*nextDeadline);
  } else {
    timer.cancel();
  }

  std::vector<Toast> const toasts = publicToasts(activeEntries);
  if (toasts.empty()) {
    if (sync.visible || isPresented) {
      hideOverlay();
    }
    sync.visible = false;
    sync.hash = 0;
    return {std::move(show), std::move(dismiss), std::move(clear), false};
  }

  std::size_t const nextHash = detail::combineHash(hashToastList(toasts), std::hash<std::uint64_t>{}(*revision));
  if (!sync.visible || !isPresented || sync.hash != nextHash) {
    showOverlay(
        Element {ToastOverlay {
            .toasts = toasts,
            .onDismiss = dismiss,
        }},
        OverlayConfig {
            .modal = false,
            .backdropColor = Colors::transparent,
            .dismissOnOutsideTap = false,
            .dismissOnEscape = false,
            .debugName = "toast",
        });
    sync.visible = true;
    sync.hash = nextHash;
  }

  return {std::move(show), std::move(dismiss), std::move(clear), true};
}

} // namespace flux
