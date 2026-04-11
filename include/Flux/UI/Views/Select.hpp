#pragma once

/// \file Flux/UI/Views/Select.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/PopoverPlacement.hpp>

#include <functional>
#include <string>
#include <vector>

namespace flux {

/// One menu row in a \ref Select.
struct SelectOption {
  std::string label;
  std::string detail;
  bool disabled = false;
};

/// Single-select dropdown backed by a popover menu.
///
/// `selectedIndex == -1` means “no selection”, which shows \ref placeholder when present.
struct Select : ViewModifiers<Select> {
  struct Style {
    Font labelFont = kFontFromTheme;
    Font detailFont = kFontFromTheme;
    float cornerRadius = kFloatFromTheme;
    float menuCornerRadius = kFloatFromTheme;
    float menuMaxHeight = kFloatFromTheme;
    float minMenuWidth = kFloatFromTheme;
    Color accentColor = kColorFromTheme;
    Color fieldColor = kColorFromTheme;
    Color fieldHoverColor = kColorFromTheme;
    Color borderColor = kColorFromTheme;
    Color rowHoverColor = kColorFromTheme;
  };

  /// Controlled selection state. When omitted, the control manages its own local selection.
  State<int> selectedIndex {};

  std::vector<SelectOption> options;

  std::string placeholder = "Select an option";
  std::string helperText;
  std::string emptyText = "No options available";

  bool disabled = false;
  bool showCheckmark = true;
  bool dismissOnSelect = true;

  PopoverPlacement placement = PopoverPlacement::Below;
  Style style {};

  /// Called after user selection changes. Receives the selected option index.
  std::function<void(int)> onChange;

  Element body() const;
};

} // namespace flux
