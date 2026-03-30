#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/Graphics/Path.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Views/ForEach.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/PathShape.hpp>
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

namespace detail {

inline Path chevronPath(bool open, float w, float h) {
  Path p;
  if (!open) {
    p.moveTo({0.f, 0.f});
    p.lineTo({w, 0.f});
    p.lineTo({w * 0.5f, h});
    p.close();
  } else {
    p.moveTo({0.f, h});
    p.lineTo({w, h});
    p.lineTo({w * 0.5f, 0.f});
    p.close();
  }
  return p;
}

/// Scale factor vs 16pt reference (body text). Used for checkmark path and column width.
inline float checkmarkScale(Font const& font) {
  constexpr float kRefFontSize = 16.f;
  float const fs = font.size > 0.f ? font.size : kRefFontSize;
  return fs / kRefFontSize;
}

/// Open polyline for a ✓ (stroke only — filling a 3-point path fills a triangle).
/// Geometry is authored at 16pt text size; \p scale is `font.size / 16` (or 1 when size unset).
inline Path checkmarkPath(float scale) {
  Path p;
  constexpr float baseW = 20.f;
  const float s = scale;
  const float w = baseW * s;
  p.moveTo({2.4f * s, 8.1f * s});
  p.lineTo({8.5f * s, 14.2f * s});
  p.lineTo({(baseW - 0.8f) * s, 1.8f * s});
  return p;
}

inline PathShape checkmarkPathShape(Color c, Font const& font) {
  float const scale = checkmarkScale(font);
  StrokeStyle ss = StrokeStyle::solid(c, std::max(1.5f, 4.0f * scale));
  ss.join = StrokeJoin::Round;
  ss.cap = StrokeCap::Round;
  return PathShape{
      .path = checkmarkPath(scale),
      .fill = FillStyle::none(),
      .stroke = std::move(ss),
  };
}

} // namespace detail

/// Background corners for a menu row so fills stay inside the popover’s rounded outline.
inline CornerRadius pickerMenuRowCorners(std::size_t rowIndex, std::size_t rowCount, float r) {
  if (rowCount == 0 || r <= 0.f) {
    return CornerRadius{};
  }
  if (rowCount == 1) {
    return CornerRadius{r};
  }
  if (rowIndex == 0) {
    return CornerRadius{r, r, 0.f, 0.f};
  }
  if (rowIndex == rowCount - 1) {
    return CornerRadius{0.f, 0.f, r, r};
  }
  return CornerRadius{};
}

template<typename T>
struct PickerOption {
  T value{};
  std::string label;
};

template<typename T>
struct PickerRow {
  PickerOption<T> option{};
  bool selected = false;
  bool keyboardActive = false;
  /// Matches \ref Picker::paddingH so menu labels align with the trigger text.
  float rowPaddingH = 12.f;
  /// Corners rounded where the row meets the popover card (see \ref pickerMenuRowCorners).
  CornerRadius rowBgCorners{};
  Font font{};
  Color textColor{};
  Color hoverColor{};
  Color selectedColor{};
  Color checkmarkColor{};
  std::function<void()> onSelect;

  auto body() const {
    bool const hovered = useHover();

    Color const bg = keyboardActive ? hoverColor
                   : hovered        ? hoverColor
                   : selected       ? selectedColor
                                    : Colors::transparent;

    return ZStack{
        .children =
            {
                Rectangle{
                    .frame = {0.f, 0.f, 0.f, 36.f},
                    .cornerRadius = rowBgCorners,
                    .fill = FillStyle::solid(bg),
                    .stroke = StrokeStyle::none(),
                    .flexGrow = 1.f,
                    .onTap = onSelect,
                    .cursor = Cursor::Hand,
                },
                HStack{
                    .spacing = 0.f,
                    .vAlign = VerticalAlignment::Center,
                    .children =
                        {
                            Rectangle{
                                .frame = {0.f, 0.f, rowPaddingH, 36.f},
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                            },
                            Element{HStack{
                                .spacing = 8.f,
                                .vAlign = VerticalAlignment::Center,
                                .children =
                                    {
                                        Text{
                                            .text = option.label,
                                            .font = font,
                                            .color = textColor,
                                            .flexGrow = 1.f,
                                            .onTap = onSelect,
                                            .cursor = Cursor::Hand,
                                        },
                                        ZStack{
                                            .children =
                                                {
                                                    Rectangle{.frame = {0.f, 0.f, std::max(12.f, 20.f * detail::checkmarkScale(font)), 36.f}},
                                                    selected ? Element{detail::checkmarkPathShape(checkmarkColor, font)}
                                                             : Element{Rectangle{}},
                                                },
                                        },
                                    },
                            }}
                                .withFlex(1.f),
                            Rectangle{
                                .frame = {0.f, 0.f, rowPaddingH, 36.f},
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                            },
                        },
                },
            },
    };
  }
};

namespace detail {

/// Builds dropdown content without a `Picker*` — tap/key handlers must not call member functions on a
/// potentially stale `this` (segfault); all menu/state/styling is passed by value.
template<typename T>
  requires std::equality_comparable<T>
Popover makePickerDropdownPopover(std::vector<PickerOption<T>> opts, std::function<void()> hide,
                                  State<T> val, State<int> keyboardCursor, float triggerWidth,
                                  float triggerRowHeight, float maxDropdownHeight, float rowPaddingH,
                                  CornerRadius menuCornerRadius,
                                  std::function<void(T const&)> onCh, Color rowHoverColor,
                                  Color rowSelectedColor, Font font, Color textColor, Color checkColor) {
  std::vector<int> indices(opts.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::size_t const rowCount = opts.size();
  float const menuR =
      std::min({menuCornerRadius.topLeft, menuCornerRadius.topRight, menuCornerRadius.bottomRight,
                menuCornerRadius.bottomLeft});

  Element rowList = VStack{
      .spacing = 0.f,
      .clip = true,
      .children =
          {
              Element{ForEach<int>{
                  std::move(indices),
                  [opts, val, hide, onCh, keyboardCursor, rowPaddingH, rowCount, menuR,
                   rowHoverColor = rowHoverColor, rowSelectedColor = rowSelectedColor, font = font,
                   textColor = textColor, checkColor = checkColor](int i) -> Element {
                    auto const idx = static_cast<std::size_t>(i);
                    return Element{PickerRow<T>{
                        .option = opts[idx],
                        .selected = (opts[idx].value == *val),
                        .keyboardActive = (*keyboardCursor == i),
                        .rowPaddingH = rowPaddingH,
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
                    }};
                  },
                  0.f,
              }},
          },
  };

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
      .anchorMaxHeight = triggerRowHeight,
      .dismissOnEscape = true,
      .dismissOnOutsideTap = true,
  };
}

} // namespace detail

template<typename T>
  requires std::equality_comparable<T>
struct Picker {
  // ── Content ──────────────────────────────────────────────────────────────

  State<T> value{};
  std::vector<PickerOption<T>> options;
  std::string placeholder = "Select…";

  // ── Appearance ───────────────────────────────────────────────────────────

  Font font{ .size = 15.f, .weight = 400.f };

  Color textColor = Color::hex(0x111118);
  Color placeholderColor = Color::hex(0xAAAAAA);
  Color backgroundColor = Color::hex(0xFFFFFF);
  Color borderColor = Color::hex(0xC8C8D0);
  Color borderFocusColor = Color::hex(0x3A7BD5);
  Color chevronColor = Color::hex(0x8E8E9A);
  Color disabledColor = Color::hex(0xDDDDDD);

  Color rowHoverColor = Color::hex(0xF0F0F5);
  Color rowSelectedColor = Color{0.23f, 0.48f, 0.84f, 0.12f};

  float borderWidth = 1.f;
  float borderFocusWidth = 2.f;
  CornerRadius cornerRadius{8.f};
  float height = 0.f;
  float paddingH = 12.f;
  float paddingV = 8.f;

  float maxDropdownHeight = 240.f;

  // ── Layout ───────────────────────────────────────────────────────────────

  float flexGrow = 0.f;
  float flexShrink = 0.f;
  float minSize = 0.f;

  // ── Behaviour ────────────────────────────────────────────────────────────

  bool disabled = false;
  std::function<void(T const&)> onChange;

  Element buildChevron(bool isOpen) const {
    float const w = 10.f;
    float const h = 5.f;
    return Element{PathShape{
        .path = detail::chevronPath(isOpen, w, h),
        .fill = FillStyle::solid(chevronColor),
        .stroke = StrokeStyle::none(),
    }};
  }

  Element body() const {
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
    float const rowPadH = paddingH;
    CornerRadius const menuCorners = cornerRadius;
    Color const rowHover = rowHoverColor;
    Color const rowSelected = rowSelectedColor;
    Font const rowFont = font;
    Color const rowText = textColor;
    Color const checkCol = borderFocusColor;

    std::optional<Rect> const layoutRect = useLayoutRect();
    float const triggerWidth = layoutRect ? layoutRect->width : 200.f;

    auto fillAnim = useAnimated<Color>(backgroundColor);
    {
      Color const target = isDisabled ? disabledColor
                         : isOpen    ? backgroundColor
                         : hovered   ? Color::hex(0xF8F8FA)
                                     : backgroundColor;
      if (*fillAnim != target) {
        fillAnim.set(target, Transition::ease(0.12f));
      }
    }

    bool const showFocusBorder = (isOpen || (focused && kbFocus)) && !isDisabled;
    Color const borderCol = showFocusBorder ? borderFocusColor : borderColor;
    float const borderW = showFocusBorder ? borderFocusWidth : borderWidth;

    float const h = height > 0.f ? height : font.size + 2.f * paddingV;

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
                                                      triggerWidth, h, maxDdH, rowPadH, menuCorners, onChangeFn,
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
                                                        triggerWidth, h, maxDdH, rowPadH, menuCorners,
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
                    .frame = {0.f, 0.f, 0.f, h},
                    .cornerRadius = cornerRadius,
                    .fill = FillStyle::solid(*fillAnim),
                    .stroke = StrokeStyle::solid(borderCol, borderW),
                    .flexGrow = flexGrow,
                    .flexShrink = flexShrink,
                    .minSize = minSize,
                    .onTap = isDisabled ? nullptr : std::function<void()>{onTriggerTap},
                    .focusable = !isDisabled,
                    .onKeyDown = isDisabled ? nullptr : std::function<void(KeyCode, Modifiers)>{onTriggerKey},
                    .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
                },
                HStack{
                    .spacing = 0.f,
                    .vAlign = VerticalAlignment::Center,
                    .children =
                        {
                            Rectangle{
                                .frame = {0.f, 0.f, paddingH, h},
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                            },
                            Text{
                                .text = hasMatch ? selectedLabel : placeholder,
                                .font = font,
                                .color = hasMatch ? textColor : placeholderColor,
                                .horizontalAlignment = HorizontalAlignment::Leading,
                                .verticalAlignment = VerticalAlignment::Center,
                                .wrapping = TextWrapping::NoWrap,
                                .padding = 0.f,
                                .cornerRadius = {},
                                .lineHeight = 0.f,
                                .maxLines = 1,
                                .firstBaselineOffset = 0.f,
                                .frame = {0.f, 0.f, 0.f, h},
                                .flexGrow = 1.f,
                                .onTap = isDisabled ? nullptr : std::function<void()>{onTriggerTap},
                                .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
                            },
                            Rectangle{
                                .frame = {0.f, 0.f, paddingH, h},
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                                .onTap = isDisabled ? nullptr : std::function<void()>{onTriggerTap},
                                .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
                            },
                            Element{ZStack{
                                .hAlign = HorizontalAlignment::Center,
                                .vAlign = VerticalAlignment::Center,
                                .children =
                                    {
                                        Rectangle{
                                            .frame = {0.f, 0.f, 14.f, h},
                                            .fill = FillStyle::none(),
                                            .stroke = StrokeStyle::none(),
                                            .onTap = isDisabled ? nullptr : std::function<void()>{onTriggerTap},
                                            .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
                                        },
                                        buildChevron(isOpen),
                                    },
                            }},
                            Rectangle{
                                .frame = {0.f, 0.f, paddingH, h},
                                .fill = FillStyle::none(),
                                .stroke = StrokeStyle::none(),
                                .onTap = isDisabled ? nullptr : std::function<void()>{onTriggerTap},
                                .cursor = isDisabled ? Cursor::Inherit : Cursor::Hand,
                            },
                        },
                },
            },
    };
  }
};

} // namespace flux
