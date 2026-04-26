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

enum class SelectTriggerMode : std::uint8_t {
    Field,
    Link,
};

/// One menu row in a \ref Select.
struct SelectOption {
    std::string label;
    std::string detail;
    bool disabled = false;

    bool operator==(SelectOption const& other) const = default;
};

/// Single-select dropdown backed by a popover menu.
///
/// `selectedIndex == -1` means “no selection”, which shows \ref placeholder when present.
struct Select : ViewModifiers<Select> {
    struct Style {
        Font labelFont = Font::theme();
        Font detailFont = Font::theme();
        float cornerRadius = kFloatFromTheme;
        float menuCornerRadius = kFloatFromTheme;
        float menuMaxHeight = kFloatFromTheme;
        float menuMaxWidth = 0.f;
        float minMenuWidth = kFloatFromTheme;
        Color accentColor = Color::theme();
        Color fieldColor = Color::theme();
        Color fieldHoverColor = Color::theme();
        Color borderColor = Color::theme();
        Color rowHoverColor = Color::theme();

        bool operator==(Style const& other) const = default;
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
    bool showDetailInTrigger = true;
    bool matchTriggerWidth = true;

    SelectTriggerMode triggerMode = SelectTriggerMode::Field;

    PopoverPlacement placement = PopoverPlacement::Below;
    Style style {};

    /// Called after user selection changes. Receives the selected option index.
    std::function<void(int)> onChange;

    bool operator==(Select const& other) const {
        return selectedIndex == other.selectedIndex && options == other.options &&
               placeholder == other.placeholder && helperText == other.helperText &&
               emptyText == other.emptyText && disabled == other.disabled &&
               showCheckmark == other.showCheckmark && dismissOnSelect == other.dismissOnSelect &&
               showDetailInTrigger == other.showDetailInTrigger &&
               matchTriggerWidth == other.matchTriggerWidth && triggerMode == other.triggerMode &&
               placement == other.placement && style == other.style &&
               static_cast<bool>(onChange) == static_cast<bool>(other.onChange);
    }

    Element body() const;
};

} // namespace flux
