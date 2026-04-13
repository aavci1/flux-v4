#include <Flux/UI/Views/Select.hpp>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Reactive/Interpolatable.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Popover.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace flux {

namespace {

float singleLineTriggerContentHeight(Theme const &theme, Font const &labelFont) {
  float const themedInnerHeight = std::max(0.f, theme.controlHeightLarge - (theme.paddingFieldV * 2.f));
  float const fontDrivenHeight = std::max(labelFont.size, 18.f);
  return std::max(themedInnerHeight, fontDrivenHeight);
}

Color lighten(Color c, float t) {
  Color const w = Colors::white;
  return Color{lerp(c.r, w.r, t), lerp(c.g, w.g, t), lerp(c.b, w.b, t), c.a};
}

Color darken(Color c, float t) {
  Color const b = Colors::black;
  return Color{lerp(c.r, b.r, t), lerp(c.g, b.g, t), lerp(c.b, b.b, t), c.a};
}

Color withAlpha(Color c, float alpha) {
  c.a = alpha;
  return c;
}

bool isValidIndex(int index, std::size_t count) {
  return index >= 0 && static_cast<std::size_t>(index) < count;
}

int firstEnabledIndex(std::vector<SelectOption> const &options) {
  for (std::size_t i = 0; i < options.size(); ++i) {
    if (!options[i].disabled) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int lastEnabledIndex(std::vector<SelectOption> const &options) {
  for (int i = static_cast<int>(options.size()) - 1; i >= 0; --i) {
    if (!options[static_cast<std::size_t>(i)].disabled) {
      return i;
    }
  }
  return -1;
}

int stepEnabledIndex(std::vector<SelectOption> const &options, int current, int direction) {
  if (direction > 0) {
    for (int i = std::max(current + 1, 0); i < static_cast<int>(options.size()); ++i) {
      if (!options[static_cast<std::size_t>(i)].disabled) {
        return i;
      }
    }
    return current;
  }
  for (int i = std::min(current - 1, static_cast<int>(options.size()) - 1); i >= 0; --i) {
    if (!options[static_cast<std::size_t>(i)].disabled) {
      return i;
    }
  }
  return current;
}

struct SelectResolvedStyle {
  Font labelFont {};
  Font detailFont {};
  float cornerRadius = 0.f;
  float menuCornerRadius = 0.f;
  float menuMaxHeight = 0.f;
  float minMenuWidth = 0.f;
  Color accentColor {};
  Color fieldColor {};
  Color fieldHoverColor {};
  Color borderColor {};
  Color rowHoverColor {};
};

SelectResolvedStyle resolveStyle(Select::Style const &style, Theme const &theme) {
  return SelectResolvedStyle{
      .labelFont = resolveFont(style.labelFont, theme.fontBody),
      .detailFont = resolveFont(style.detailFont, theme.fontBodySmall),
      .cornerRadius = resolveFloat(style.cornerRadius, theme.radiusMedium),
      .menuCornerRadius = resolveFloat(style.menuCornerRadius, theme.radiusLarge),
      .menuMaxHeight = resolveFloat(style.menuMaxHeight, 260.f),
      .minMenuWidth = resolveFloat(style.minMenuWidth, 180.f),
      .accentColor = resolveColor(style.accentColor, theme.colorAccent),
      .fieldColor = resolveColor(style.fieldColor, theme.colorSurfaceField),
      .fieldHoverColor = resolveColor(style.fieldHoverColor, theme.colorSurfaceHover),
      .borderColor = resolveColor(style.borderColor, theme.colorBorder),
      .rowHoverColor = resolveColor(style.rowHoverColor, theme.colorSurfaceRowHover),
  };
}

struct SelectMenuRow : ViewModifiers<SelectMenuRow> {
  SelectOption option;
  bool selected = false;
  bool showCheckmark = true;
  SelectResolvedStyle style {};
  Theme theme {};
  std::function<void()> onTap;

  Element body() const {
    bool const disabled = option.disabled;
    bool const hovered = useHover();
    bool const pressed = usePress();
    bool const focused = useFocus();

    Color const selectedFill = withAlpha(style.accentColor, 0.16f);
    Color const selectedHoverFill = withAlpha(style.accentColor, 0.24f);
    Color const selectedPressFill = withAlpha(style.accentColor, 0.30f);
    Color const idleFill = Colors::transparent;
    Color const hoverFill = style.rowHoverColor;
    Color const pressFill = darken(style.rowHoverColor, 0.04f);

    Color const fillTarget =
        disabled ? Colors::transparent
                 : selected ? (pressed ? selectedPressFill : hovered ? selectedHoverFill : selectedFill)
                            : (pressed ? pressFill : hovered ? hoverFill : idleFill);
    Color const labelTarget =
        disabled ? theme.colorTextDisabled : selected ? style.accentColor : theme.colorTextPrimary;
    Color const detailTarget =
        disabled ? theme.colorTextDisabled : selected ? lighten(style.accentColor, 0.08f) : theme.colorTextSecondary;
    Color const iconTarget = disabled ? theme.colorTextDisabled : style.accentColor;

    bool const hasDetail = !option.detail.empty();
    Element textBlock = Text{
        .text = option.label,
        .font = style.labelFont,
        .color = labelTarget,
        .horizontalAlignment = HorizontalAlignment::Leading,
        .verticalAlignment = VerticalAlignment::Center,
    };
    if (hasDetail) {
      std::vector<Element> textChildren;
      textChildren.reserve(2);
      textChildren.emplace_back(Text{
          .text = option.label,
          .font = style.labelFont,
          .color = labelTarget,
          .horizontalAlignment = HorizontalAlignment::Leading,
          .verticalAlignment = VerticalAlignment::Center,
      });
      textChildren.emplace_back(Text{
          .text = option.detail,
          .font = style.detailFont,
          .color = detailTarget,
          .horizontalAlignment = HorizontalAlignment::Leading,
          .verticalAlignment = VerticalAlignment::Center,
          .wrapping = TextWrapping::Wrap,
      });
      textBlock = VStack{
          .spacing = theme.space1 * 0.5f,
          .alignment = Alignment::Start,
          .children = std::move(textChildren),
      };
    }

    std::vector<Element> rowChildren;
    rowChildren.reserve(showCheckmark ? 2 : 1);
    rowChildren.emplace_back(std::move(textBlock).flex(1.f, 1.f, 0.f));
    if (showCheckmark && selected) {
      rowChildren.emplace_back(Icon{
          .name = IconName::Check,
          .size = 18.f,
          .color = iconTarget,
      });
    }

    auto activate = [onTap = onTap, disabled]() {
      if (disabled) {
        return;
      }
      if (onTap) {
        onTap();
      }
    };
    auto handleKey = [activate](KeyCode key, Modifiers) {
      if (key == keys::Return || key == keys::Space) {
        activate();
      }
    };

    return HStack{
        .spacing = theme.space3,
        .alignment = Alignment::Center,
        .children = std::move(rowChildren),
    }
        .padding(theme.space2, theme.space3, theme.space2, theme.space3)
        .fill(FillStyle::solid(fillTarget))
        .stroke(focused && !disabled ? StrokeStyle::solid(theme.colorBorderFocus, 2.f) : StrokeStyle::none())
        .cornerRadius(CornerRadius{style.cornerRadius})
        .cursor(disabled ? Cursor::Inherit : Cursor::Hand)
        .focusable(!disabled)
        .onKeyDown(disabled ? std::function<void(KeyCode, Modifiers)>{} : std::function<void(KeyCode, Modifiers)>{handleKey})
        .onTap(disabled ? std::function<void()>{} : std::function<void()>{activate});
  }
};

struct SelectMenuContent {
  State<int> selectedIndex {};
  std::vector<SelectOption> options;
  std::string emptyText;
  bool showCheckmark = true;
  float menuWidth = 0.f;
  SelectResolvedStyle style {};
  Theme theme {};
  std::function<void(int)> onSelect;

  Element body() const {
    if (options.empty()) {
      return Text{
          .text = emptyText,
          .font = style.detailFont,
          .color = theme.colorTextSecondary,
          .horizontalAlignment = HorizontalAlignment::Leading,
          .wrapping = TextWrapping::Wrap,
      }
          .padding(theme.space3)
          .width(menuWidth);
    }

    std::vector<Element> rows;
    rows.reserve(options.size());
    int const current = *selectedIndex;
    for (std::size_t i = 0; i < options.size(); ++i) {
      int const index = static_cast<int>(i);
      rows.emplace_back(SelectMenuRow{
          .option = options[i],
          .selected = current == index,
          .showCheckmark = showCheckmark,
          .style = style,
          .theme = theme,
          .onTap = [onSelect = onSelect, index] {
            if (onSelect) {
              onSelect(index);
            }
          },
      });
    }

    return ScrollView{
        .axis = ScrollAxis::Vertical,
        .children = children(
            VStack{
                .spacing = theme.space1 * 0.5f,
                .alignment = Alignment::Start,
                .children = std::move(rows),
            }
                .padding(theme.space1)
        ),
    }
        .width(menuWidth)
        .clipContent(true);
  }
};

struct SelectTrigger : ViewModifiers<SelectTrigger> {
  State<int> selectedIndex {};
  std::vector<SelectOption> options;
  std::string placeholder;
  std::string emptyText;
  bool disabled = false;
  bool showCheckmark = true;
  bool dismissOnSelect = true;
  PopoverPlacement placement = PopoverPlacement::Below;
  SelectResolvedStyle style {};
  std::function<void(int)> onChange;

  Element body() const {
    Theme const &theme = useEnvironment<Theme>();

    auto [showPopover, hidePopover, isPresented] = usePopover();

    int const currentIndex = *selectedIndex;
    SelectOption const *currentOption =
        isValidIndex(currentIndex, options.size()) ? &options[static_cast<std::size_t>(currentIndex)] : nullptr;

    bool const isDisabled = disabled;
    bool const hovered = useHover();
    bool const pressed = usePress();
    bool const focused = useFocus();
    bool const open = isPresented;
    Rect const bounds = useBounds();

    Transition const trInstant = Transition::instant();
    Transition const trMotion = theme.reducedMotion ? trInstant : Transition::ease(theme.durationMedium);
    Transition const trFast = theme.reducedMotion ? trInstant : Transition::ease(theme.durationFast);

    Color const idleFill = isDisabled ? theme.colorSurfaceDisabled : style.fieldColor;
    Color const hoverFill = isDisabled ? theme.colorSurfaceDisabled : style.fieldHoverColor;
    Color const pressFill = isDisabled ? theme.colorSurfaceDisabled : darken(style.fieldHoverColor, 0.03f);
    Color const openFill = isDisabled ? theme.colorSurfaceDisabled : lighten(style.fieldHoverColor, 0.02f);
    Color const fillTarget = pressed ? pressFill : open ? openFill : hovered ? hoverFill : idleFill;

    Color const labelTarget =
        isDisabled ? theme.colorTextDisabled
                   : currentOption ? theme.colorTextPrimary : theme.colorTextPlaceholder;
    Color const detailTarget = isDisabled ? theme.colorTextDisabled : theme.colorTextSecondary;
    Color const chevronTarget =
        isDisabled ? theme.colorTextDisabled : open ? style.accentColor : theme.colorTextSecondary;

    auto fillAnim = useAnimated<Color>(fillTarget);
    if (*fillAnim != fillTarget) {
      fillAnim.set(fillTarget, trFast);
    }

    auto labelAnim = useAnimated<Color>(labelTarget);
    if (*labelAnim != labelTarget) {
      labelAnim.set(labelTarget, trMotion);
    }

    auto detailAnim = useAnimated<Color>(detailTarget);
    if (*detailAnim != detailTarget) {
      detailAnim.set(detailTarget, trMotion);
    }

    auto chevronAnim = useAnimated<Color>(chevronTarget);
    if (*chevronAnim != chevronTarget) {
      chevronAnim.set(chevronTarget, trMotion);
    }

    float const triggerWidth = bounds.width > 0.f ? bounds.width : style.minMenuWidth;
    EdgeInsets const anchorOutsets{
        theme.paddingFieldV,
        theme.paddingFieldH,
        theme.paddingFieldV,
        theme.paddingFieldH,
    };
    float const menuWidth =
        std::max(triggerWidth + anchorOutsets.left + anchorOutsets.right, style.minMenuWidth);
    std::optional<float> const anchorMaxHeight =
        bounds.height > 0.f ? std::optional<float>{bounds.height} : std::nullopt;

    auto applySelection = [selectedIndex = selectedIndex, options = options, onChange = onChange](int nextIndex) {
      if (!isValidIndex(nextIndex, options.size()) || options[static_cast<std::size_t>(nextIndex)].disabled) {
        return;
      }
      if (*selectedIndex != nextIndex) {
        selectedIndex = nextIndex;
        if (onChange) {
          onChange(nextIndex);
        }
      }
    };

    auto openMenu = [showPopover,
                     hidePopover,
                     selectedIndex = selectedIndex,
                     options = options,
                     emptyText = emptyText,
                     showCheckmark = showCheckmark,
                     dismissOnSelect = dismissOnSelect,
                     style = style,
                     theme,
                     menuWidth,
                     placement = placement,
                     anchorMaxHeight,
                     anchorOutsets,
                     onChange = onChange]() {
      auto handleSelect = [selectedIndex, options = options, dismissOnSelect, hidePopover, onChange](int nextIndex) {
        if (!isValidIndex(nextIndex, options.size()) || options[static_cast<std::size_t>(nextIndex)].disabled) {
          return;
        }
        if (*selectedIndex != nextIndex) {
          selectedIndex = nextIndex;
          if (onChange) {
            onChange(nextIndex);
          }
        }
        if (dismissOnSelect) {
          hidePopover();
        }
      };

      showPopover(Popover{
          .content = Element{SelectMenuContent{
              .selectedIndex = selectedIndex,
              .options = options,
              .emptyText = emptyText,
              .showCheckmark = showCheckmark,
              .menuWidth = menuWidth,
              .style = style,
              .theme = theme,
              .onSelect = handleSelect,
          }},
          .placement = placement,
          .arrow = false,
          .backgroundColor = theme.colorSurfaceOverlay,
          .borderColor = theme.colorBorderSubtle,
          .borderWidth = 1.f,
          .cornerRadius = style.menuCornerRadius,
          .contentPadding = 0.f,
          .maxSize = Size{menuWidth, style.menuMaxHeight},
          .backdropColor = Colors::transparent,
          .anchorMaxHeight = anchorMaxHeight,
          .anchorOutsets = anchorOutsets,
          .dismissOnEscape = true,
          .dismissOnOutsideTap = true,
          .useTapAnchor = false,
      });
    };

    auto moveSelection = [applySelection, options = options, selectedIndex = selectedIndex](int direction) {
      int const next = stepEnabledIndex(options, *selectedIndex, direction);
      if (next != *selectedIndex) {
        applySelection(next);
      }
    };

    auto jumpSelection = [applySelection, options = options](bool toEnd) {
      int const next = toEnd ? lastEnabledIndex(options) : firstEnabledIndex(options);
      if (next >= 0) {
        applySelection(next);
      }
    };

    auto toggleMenu = [isDisabled, open, openMenu, hidePopover]() {
      if (isDisabled) {
        return;
      }
      if (open) {
        hidePopover();
      } else {
        openMenu();
      }
    };

    auto handleKey = [isDisabled, open, toggleMenu, hidePopover, moveSelection, jumpSelection](KeyCode key, Modifiers) {
      if (isDisabled) {
        return;
      }
      if (key == keys::Return || key == keys::Space) {
        toggleMenu();
        return;
      }
      if (key == keys::Escape && open) {
        hidePopover();
        return;
      }
      if (key == keys::DownArrow) {
        moveSelection(1);
        return;
      }
      if (key == keys::UpArrow) {
        moveSelection(-1);
        return;
      }
      if (key == keys::Home) {
        jumpSelection(false);
        return;
      }
      if (key == keys::End) {
        jumpSelection(true);
      }
    };

    bool const hasDetail = currentOption && !currentOption->detail.empty();
    Element triggerLabel = Text{
        .text = currentOption ? currentOption->label : placeholder,
        .font = style.labelFont,
        .color = *labelAnim,
        .horizontalAlignment = HorizontalAlignment::Leading,
        .verticalAlignment = VerticalAlignment::Center,
        .wrapping = TextWrapping::Wrap,
    };

    Element triggerTextBlock = ZStack{
        .horizontalAlignment = Alignment::Start,
        .verticalAlignment = Alignment::Center,
        .children = children(std::move(triggerLabel)),
    }
        .height(singleLineTriggerContentHeight(theme, style.labelFont));
    if (hasDetail) {
      std::vector<Element> triggerTextChildren;
      triggerTextChildren.reserve(2);
      triggerTextChildren.emplace_back(Text{
          .text = currentOption ? currentOption->label : placeholder,
          .font = style.labelFont,
          .color = *labelAnim,
          .horizontalAlignment = HorizontalAlignment::Leading,
          .verticalAlignment = VerticalAlignment::Center,
          .wrapping = TextWrapping::Wrap,
      });
      triggerTextChildren.emplace_back(Text{
          .text = currentOption->detail,
          .font = style.detailFont,
          .color = *detailAnim,
          .horizontalAlignment = HorizontalAlignment::Leading,
          .verticalAlignment = VerticalAlignment::Center,
          .wrapping = TextWrapping::Wrap,
          .maxLines = 2,
      });
      triggerTextBlock = VStack{
          .spacing = theme.space1 * 0.5f,
          .alignment = Alignment::Start,
          .children = std::move(triggerTextChildren),
      };
    }

    return HStack{
        .spacing = theme.space3,
        .alignment = Alignment::Center,
        .children = children(
            std::move(triggerTextBlock).flex(1.f, 1.f, 0.f),
            Icon{
                .name = open ? IconName::KeyboardArrowUp : IconName::KeyboardArrowDown,
                .size = 18.f,
                .color = *chevronAnim,
            })
    }
        .padding(theme.paddingFieldV, theme.paddingFieldH, theme.paddingFieldV, theme.paddingFieldH)
        .fill(FillStyle::solid(*fillAnim))
        .stroke(focused && !isDisabled
                    ? StrokeStyle::solid(theme.colorBorderFocus, 2.f)
                    : StrokeStyle::solid(open && !isDisabled ? style.accentColor : style.borderColor, 1.f))
        .cornerRadius(CornerRadius{style.cornerRadius})
        .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
        .focusable(!isDisabled)
        .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)>{}
                              : std::function<void(KeyCode, Modifiers)>{handleKey})
        .onTap(isDisabled ? std::function<void()>{} : std::function<void()>{toggleMenu});
  }
};

} // namespace

Element Select::body() const {
  Theme const &theme = useEnvironment<Theme>();
  SelectResolvedStyle const resolved = resolveStyle(style, theme);

  State<int> const selection = selectedIndex.signal ? selectedIndex : useState<int>(-1);
  Element field = SelectTrigger{
      .selectedIndex = selection,
      .options = options,
      .placeholder = placeholder,
      .emptyText = emptyText,
      .disabled = disabled,
      .showCheckmark = showCheckmark,
      .dismissOnSelect = dismissOnSelect,
      .placement = placement,
      .style = resolved,
      .onChange = onChange,
  };

  if (helperText.empty()) {
    return field;
  }

  return VStack{
      .spacing = theme.space1,
      .alignment = Alignment::Start,
      .children = children(
          std::move(field),
          Text{
              .text = helperText,
              .font = resolved.detailFont,
              .color = disabled ? theme.colorTextDisabled : theme.colorTextSecondary,
              .horizontalAlignment = HorizontalAlignment::Leading,
              .wrapping = TextWrapping::Wrap,
          })
  };
}

} // namespace flux
