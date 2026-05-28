#pragma once

/// \file Lambda/UI/Views/ListView.hpp
///
/// Part of the Lambda public API.

#include <Lambda/UI/Element.hpp>
#include <Lambda/UI/Hooks.hpp>
#include <Lambda/UI/Theme.hpp>

#include <functional>
#include <vector>

namespace lambda {

struct ListRow : ViewModifiers<ListRow> {
    struct Style {
        /// Horizontal inset inside the row.
        float paddingH = kFloatFromTheme;
        /// Vertical inset inside the row.
        float paddingV = kFloatFromTheme;

        bool operator==(Style const& other) const = default;
    };

    /// Row body content.
    Element content;
    /// Selected state for styling.
    bool selected = false;
    /// Prevents interaction when true.
    bool disabled = false;
    /// Optional row token overrides.
    Style style {};
    /// Called when the row is activated.
    std::function<void()> onTap;

    Element body() const;
};

struct ListView : ViewModifiers<ListView> {
    struct Style {
        /// Horizontal inset applied to row dividers.
        float dividerInsetH = kFloatFromTheme;

        bool operator==(Style const& other) const = default;
    };

    /// Row elements in order.
    std::vector<Element> rows;
    /// Draws row separators when true.
    bool showDividers = true;
    /// Optional list token overrides.
    Style style {};

    Element body() const;
};

} // namespace lambda
