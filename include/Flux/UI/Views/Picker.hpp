#pragma once

/// \file Flux/UI/Views/Picker.hpp
///
/// Part of the Flux public API.


/// Dropdown / combo-box style picker. \c Picker<T> is a template (options carry values of type \c T);
/// its \c body() and menu row UI are implemented in this header so instantiations see the full
/// definition. Only \ref pickerMenuRowCorners is compiled in \c Picker.cpp.
///
/// \c T must be equality-comparable (\c ==) so the control can match the current \ref value to labels.

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/InputFieldLayout.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/ForEach.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Popover.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <algorithm>
#include <concepts>
#include <functional>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

namespace flux {

/// Background corners for a menu row so fills stay inside the popover’s rounded outline.
CornerRadius pickerMenuRowCorners(std::size_t rowIndex, std::size_t rowCount, float r);

/// One selectable entry in a \ref Picker list.
template<typename T>
struct PickerOption {
  T value{};
  std::string label;
};

/// One row in the dropdown menu (built by \ref detail::makePickerDropdownPopover).
template<typename T>
struct PickerRow : ViewModifiers<PickerRow<T>> {
  // ── Row data ─────────────────────────────────────────────────────────────

  PickerOption<T> option{};
  bool selected = false;
  bool keyboardActive = false;

  // ── Layout (aligned with trigger) ───────────────────────────────────────

  /// Matches \ref Picker::paddingH so menu labels align with the trigger text.
  float rowPaddingH = 12.f;
  /// Matches the picker trigger row height (\ref Picker resolved height).
  float rowHeight = 36.f;
  /// Corners rounded where the row meets the popover card (see \ref pickerMenuRowCorners).
  CornerRadius rowBgCorners{};
  Font font{};
  Color textColor{};
  Color hoverColor{};
  Color selectedColor{};
  Color checkmarkColor{};
  std::function<void()> onSelect;

  // ── Component protocol ───────────────────────────────────────────────────

  Element body() const {
    bool const hovered = useHover();
    FluxTheme const& theme = useEnvironment<FluxTheme>();
    float const iconSz = resolveFloat(kFloatFromTheme, theme.typeBody.size);
    float const checkColW = std::max(12.f, iconSz);

    Color const bg = keyboardActive ? hoverColor
                   : hovered        ? hoverColor
                   : selected       ? selectedColor
                                    : Colors::transparent;

    return ZStack{
        // Row background + row content overlay: same origin (Leading/Top).
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Top,
        .children =
            {
                Rectangle{
                    .fill = FillStyle::solid(bg),
                    .stroke = StrokeStyle::none(),
                }
                    .height(rowHeight)
                    .cursor(Cursor::Hand)
                    .onTap(onSelect)
                    .cornerRadius(rowBgCorners)
                    .flex(1.f),
                HStack{
                    .spacing = 0.f,
                    .vAlign = VerticalAlignment::Center,
                    .children =
                        {
                            Rectangle{
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                            }
                                .size(rowPaddingH, rowHeight),
                            HStack{
                                .spacing = 8.f,
                                .vAlign = VerticalAlignment::Center,
                                .children =
                                    {
                                        Text{
                                            .text = option.label,
                                            .font = font,
                                            .color = textColor,
                                            .horizontalAlignment = HorizontalAlignment::Leading,
                                            .verticalAlignment = VerticalAlignment::Center,
                                            .wrapping = TextWrapping::NoWrap,
                                            .lineHeight = 0.f,
                                            .maxLines = 1,
                                            .firstBaselineOffset = 0.f,
                                        }
                                            .size(0.f, rowHeight)
                                            .flex(1.f),
                                        ZStack{
                                            .hAlign = HorizontalAlignment::Leading,
                                            .vAlign = VerticalAlignment::Top,
                                            .children =
                                                {
                                                    Rectangle{}.size(checkColW, rowHeight),
                                                    selected ? Element{Icon{
                                                                   .name = IconName::Check,
                                                                   .size = iconSz,
                                                                   .color = checkmarkColor,
                                                               }}
                                                             : Element{Rectangle{}},
                                                },
                                        },
                                    },
                            }
                                .flex(1.f),
                            Rectangle{
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                            }
                                .size(rowPaddingH, rowHeight),
                        },
                },
            },
    };
  }
};

namespace detail {

/// Builds dropdown content without a `Picker*` — the `Picker` value is only live for the duration of
/// `body()`; capturing `this` in lambdas would dangle. Handlers close over values captured by copy
/// (State handles, options, colors, etc.).
template<typename T>
  requires std::equality_comparable<T>
Popover makePickerDropdownPopover(std::vector<PickerOption<T>> opts, std::function<void()> hide,
                                  State<T> val, State<int> keyboardCursor, float triggerWidth,
                                  float triggerRowHeight, float maxDropdownHeight, float rowPaddingH,
                                  float menuCornerRadius,
                                  std::function<void(T const&)> onCh, Color rowHoverColor,
                                  Color rowSelectedColor, Font font, Color textColor, Color checkColor) {
  std::vector<int> indices(opts.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::size_t const rowCount = opts.size();
  float const menuR = menuCornerRadius;

  Element rowList = VStack{
      .spacing = 0.f,
      .children =
          {
              ForEach<int>{
                  std::move(indices),
                  [opts, val, hide, onCh, keyboardCursor, rowPaddingH, rowCount, menuR, triggerRowHeight,
                   rowHoverColor = rowHoverColor, rowSelectedColor = rowSelectedColor, font = font,
                   textColor = textColor, checkColor = checkColor](int i) -> Element {
                    auto const idx = static_cast<std::size_t>(i);
                    return PickerRow<T>{
                        .option = opts[idx],
                        .selected = (opts[idx].value == *val),
                        .keyboardActive = (*keyboardCursor == i),
                        .rowPaddingH = rowPaddingH,
                        .rowHeight = triggerRowHeight,
                        .rowBgCorners = pickerMenuRowCorners(idx, rowCount, menuR),
                        .font = font,
                        .textColor = textColor,
                        .hoverColor = rowHoverColor,
                        .selectedColor = rowSelectedColor,
                        .checkmarkColor = checkColor,
                        .onSelect =
                            [val, hide, onCh, v = opts[idx].value]() {
                              val = v;
                              if (onCh) {
                                onCh(v);
                              }
                              hide();
                            },
                    };
                  },
                  0.f,
              },
          },
  }.clipContent(true);

  Size dropdownMax{};
  dropdownMax.width = std::max(triggerWidth, 8.f);
  dropdownMax.height = maxDropdownHeight > 0.f ? maxDropdownHeight : 240.f;
  return Popover{
      .content = std::move(rowList),
      .placement = PopoverPlacement::Below,
      .gap = 4.f,
      .arrow = false,
      .cornerRadius = menuCornerRadius,
      .contentPadding = 0.f,
      .maxSize = std::make_optional(dropdownMax),
      .backdropColor = Colors::transparent,
      .anchorMaxHeight = triggerRowHeight,
      .dismissOnEscape = true,
      .dismissOnOutsideTap = true,
  };
}

} // namespace detail

/// Field-style dropdown: tap opens a \ref Popover menu; keyboard navigates rows when open.
template<typename T>
  requires std::equality_comparable<T>
struct Picker : ViewModifiers<Picker<T>> {
  // ── Content ──────────────────────────────────────────────────────────────

  /// Bound selection; must stay in sync with \ref options entries.
  State<T> value{};
  std::vector<PickerOption<T>> options;
  /// Shown when no option matches \c *value or the field is empty.
  std::string placeholder = "Select…";

  // ── Appearance ───────────────────────────────────────────────────────────

  Font font = kFontFromTheme;

  Color textColor = kFromTheme;
  Color placeholderColor = kFromTheme;
  Color backgroundColor = kFromTheme;
  Color borderColor = kFromTheme;
  Color borderFocusColor = kFromTheme;
  Color chevronColor = kFromTheme;
  Color disabledColor = kFromTheme;

  Color rowHoverColor = kFromTheme;
  Color rowSelectedColor = kFromTheme;

  float borderWidth = 1.f;
  float borderFocusWidth = 2.f;
  /// Uniform trigger corner radius (`kFloatFromTheme` = `FluxTheme::radiusMedium`); dropdown menu uses
  /// `radiusLarge` separately. Scalar only — no per-corner radii on the trigger via this field.
  float cornerRadius = kFloatFromTheme;
  /// Total trigger height; 0 = same rule as \ref TextInput (\ref resolvedInputFieldHeight).
  float height = 0.f;
  float paddingH = kFloatFromTheme;
  float paddingV = kFloatFromTheme;

  float maxDropdownHeight = 240.f;

  // ── Layout ───────────────────────────────────────────────────────────────

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  // ── Behaviour ────────────────────────────────────────────────────────────

  bool disabled = false;
  /// Fired after the user picks a value (and after \c value is updated).
  std::function<void(T const&)> onChange;

  /// Builds the trigger row, popover presentation, and input routing. Call only from a composite \c body().
  Element body() const {
    FluxTheme const& theme = useEnvironment<FluxTheme>();
    float const padHResolved = resolveFloat(paddingH, theme.paddingFieldH);
    float const padVResolved = resolveFloat(paddingV, theme.paddingFieldV);
    CornerRadius const triggerCr{resolveFloat(cornerRadius, theme.radiusMedium)};
    float const menuRadius = theme.radiusLarge;
    // Trigger background/hover cross-fade: `FluxTheme` motion tokens (cf. Button).
    Transition const trMed =
        theme.reducedMotion ? Transition::instant() : Transition::ease(theme.durationMedium);

    Font const fontR = resolveFont(font, theme.typeBody.toFont());
    Color const textR = resolveColor(textColor, theme.colorTextPrimary);
    Color const plcR = resolveColor(placeholderColor, theme.colorTextPlaceholder);
    Color const bgR = resolveColor(backgroundColor, theme.colorSurfaceField);
    Color const brdR = resolveColor(borderColor, theme.colorBorder);
    Color const brdFocusR = resolveColor(borderFocusColor, theme.colorBorderFocus);
    Color const chvR = resolveColor(chevronColor, theme.colorTextMuted);
    Color const disR = resolveColor(disabledColor, theme.colorSurfaceDisabled);
    Color const rowHoverR = resolveColor(rowHoverColor, theme.colorSurfaceRowHover);
    Color const rowSelR = resolveColor(rowSelectedColor, theme.colorAccentSubtle);
    Color const triggerHoverR = theme.colorSurfaceHover;

    auto [showPopover, hidePopover, isOpen] = usePopover();
    auto requestFocus = useRequestFocus();
    auto keyboardCursor = useState(-1);
    auto prevPopoverOpen = useState(false);
    bool const focused = useFocus();
    bool const kbFocus = useKeyboardFocus();
    bool const hovered = useHover();
    bool const isDisabled = disabled;

    if (*prevPopoverOpen && !isOpen) {
      keyboardCursor = -1;
    }
    prevPopoverOpen = isOpen;

    if (isOpen && !isDisabled && !focused) {
      hidePopover();
      keyboardCursor = -1;
    }

    std::string selectedLabel;
    bool hasMatch = false;
    for (auto const& opt : options) {
      if (opt.value == *value) {
        selectedLabel = opt.label;
        hasMatch = true;
        break;
      }
    }

    // Snapshot for event handlers: handlers must not use `this` after the build (stale pointer).
    std::vector<PickerOption<T>> const menuOptions = options;
    State<T> valHandle = value;
    std::function<void(T const&)> const onChangeFn = onChange;
    float const maxDdH = maxDropdownHeight > 0.f ? maxDropdownHeight : 240.f;
    float const rowPadH = padHResolved;
    Color const rowHover = rowHoverR;
    Color const rowSelected = rowSelR;
    Font const rowFont = fontR;
    Color const rowText = textR;
    Color const checkCol = brdFocusR;

    std::optional<Rect> const layoutRect = useLayoutRect();
    float const triggerWidth = layoutRect ? layoutRect->width : 200.f;
    float const chevronIconSz = resolveFloat(kFloatFromTheme, theme.typeBody.size);

    auto fillAnim = useAnimated<Color>(bgR);
    {
      Color const target =
          isDisabled ? disR : isOpen ? bgR : hovered ? triggerHoverR : bgR;
      if (*fillAnim != target) {
        fillAnim.set(target, trMed);
      }
    }

    bool const showFocusBorder = (isOpen || (focused && kbFocus)) && !isDisabled;
    Color const borderCol = showFocusBorder ? brdFocusR : brdR;
    float const borderW = showFocusBorder ? borderFocusWidth : borderWidth;

    float const h = resolvedInputFieldHeight(fontR, textR, padVResolved, height);

    std::size_t const optionCount = menuOptions.size();

    auto onTriggerTap = [=] {
      if (isDisabled) {
        return;
      }
      if (optionCount == 0) {
        return;
      }
      requestFocus();
      if (isOpen) {
        hidePopover();
        keyboardCursor = -1;
      } else {
        showPopover(detail::makePickerDropdownPopover(menuOptions, hidePopover, valHandle, keyboardCursor,
                                                      triggerWidth, h, maxDdH, rowPadH, menuRadius, onChangeFn,
                                                      rowHover, rowSelected, rowFont, rowText, checkCol));
      }
    };

    int const nOpts = static_cast<int>(optionCount);

    auto onTriggerKey = [=](KeyCode k, Modifiers) {
      if (isDisabled) {
        return;
      }
      if (!isOpen) {
        if (k == keys::Return || k == keys::Space) {
          onTriggerTap();
        }
        if (k == keys::DownArrow && nOpts > 0) {
          requestFocus();
          keyboardCursor = 0;
          showPopover(detail::makePickerDropdownPopover(menuOptions, hidePopover, valHandle, keyboardCursor,
                                                        triggerWidth, h, maxDdH, rowPadH, menuRadius,
                                                        onChangeFn, rowHover, rowSelected, rowFont, rowText,
                                                        checkCol));
        }
        return;
      }
      int const n = nOpts;
      if (n <= 0) {
        return;
      }
      if (k == keys::DownArrow) {
        keyboardCursor = std::min(*keyboardCursor + 1, n - 1);
        return;
      }
      if (k == keys::UpArrow) {
        keyboardCursor = std::max(*keyboardCursor - 1, 0);
        return;
      }
      if (k == keys::Return || k == keys::Space) {
        int const idx = *keyboardCursor;
        if (idx >= 0 && idx < n) {
          T const v = menuOptions[static_cast<std::size_t>(idx)].value;
          valHandle = v;
          if (onChangeFn) {
            onChangeFn(v);
          }
        }
        hidePopover();
        keyboardCursor = -1;
        return;
      }
    };

    return ZStack{
        .hAlign = HorizontalAlignment::Leading,
        .vAlign = VerticalAlignment::Center,
        .children =
            {
                Rectangle{
                    .fill = FillStyle::solid(*fillAnim),
                    .stroke = StrokeStyle::solid(borderCol, borderW),
                }
                    .height(h)
                    .cursor(isDisabled ? Cursor::Inherit : Cursor::Hand)
                    .focusable(!isDisabled)
                    .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)>{}
                                          : std::function<void(KeyCode, Modifiers)>{onTriggerKey})
                    .onTap(isDisabled ? std::function<void()>{} : std::function<void()>{onTriggerTap})
                    .cornerRadius(triggerCr)
                    .flex(flexGrow, flexShrink, minSize),
                HStack{
                    .spacing = 0.f,
                    .vAlign = VerticalAlignment::Center,
                    .children =
                        {
                            Rectangle{
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                            }
                                .size(padHResolved, h),
                            Text{
                                .text = hasMatch ? selectedLabel : placeholder,
                                .font = fontR,
                                .color = hasMatch ? textR : plcR,
                                .horizontalAlignment = HorizontalAlignment::Leading,
                                .verticalAlignment = VerticalAlignment::Center,
                                .wrapping = TextWrapping::NoWrap,
                                .lineHeight = 0.f,
                                .maxLines = 1,
                                .firstBaselineOffset = 0.f,
                            }
                                .size(0.f, h)
                                .flex(1.f),
                            Rectangle{
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                            }
                                .size(padHResolved, h),
                            ZStack{
                                .hAlign = HorizontalAlignment::Leading,
                                .vAlign = VerticalAlignment::Center,
                                .children =
                                    {
                                        Rectangle{
                                            .fill = FillStyle::none(),
                                            .stroke = StrokeStyle::none(),
                                        }
                                            .size(std::max(14.f, chevronIconSz), h),
                                        Icon{
                                          .name = isOpen ? IconName::ExpandLess : IconName::ExpandMore,
                                          .size = chevronIconSz,
                                          .color = chvR,
                                      }
                                    },
                            },
                            Rectangle{
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                            }
                                .size(padHResolved, h),
                        },
                },
            },
    };
  }
};

} // namespace flux
