#pragma once

/// \file Flux/UI/Views/SegmentedControl.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/Graphics/Font.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>
#include <string>
#include <vector>

namespace flux {

struct SegmentedControlOption {
    std::string label;
    bool disabled = false;

    bool operator==(SegmentedControlOption const& other) const = default;
};

struct SegmentedControl : ViewModifiers<SegmentedControl> {
    struct Style {
        Font font = Font::theme();
        float paddingH = kFloatFromTheme;
        float paddingV = kFloatFromTheme;
        float cornerRadius = kFloatFromTheme;
        Color accentColor = Color::theme();
        Color trackColor = Color::theme();
        Color borderColor = Color::theme();

        bool operator==(Style const& other) const = default;
    };

    State<int> selectedIndex {};
    std::vector<SegmentedControlOption> options;
    bool disabled = false;
    Style style {};
    std::function<void(int)> onChange;

    bool operator==(SegmentedControl const& other) const {
        return selectedIndex == other.selectedIndex && options == other.options &&
               disabled == other.disabled && style == other.style &&
               static_cast<bool>(onChange) == static_cast<bool>(other.onChange);
    }

    Element body() const;
};

} // namespace flux
