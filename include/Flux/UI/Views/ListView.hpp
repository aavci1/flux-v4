#pragma once

/// \file Flux/UI/Views/ListView.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <functional>
#include <vector>

namespace flux {

struct ListRow : ViewModifiers<ListRow> {
    struct Style {
        float paddingH = kFloatFromTheme;
        float paddingV = kFloatFromTheme;

        bool operator==(Style const& other) const = default;
    };

    Element content;
    bool selected = false;
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    bool operator==(ListRow const& other) const {
        return content.structuralEquals(other.content) && selected == other.selected &&
               disabled == other.disabled && style == other.style;
    }

    Element body() const;
};

struct ListView : ViewModifiers<ListView> {
    struct Style {
        float dividerInsetH = kFloatFromTheme;

        bool operator==(Style const& other) const = default;
    };

    std::vector<Element> rows;
    bool showDividers = true;
    Style style {};

    bool operator==(ListView const& other) const {
        return elementsStructurallyEqual(rows, other.rows) && showDividers == other.showDividers &&
               style == other.style;
    }

    Element body() const;
};

} // namespace flux
