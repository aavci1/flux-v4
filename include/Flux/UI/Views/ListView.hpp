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
    };

    Element content;
    bool selected = false;
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    Element body() const;
};

struct ListView : ViewModifiers<ListView> {
    struct Style {
        float dividerInsetH = kFloatFromTheme;
    };

    std::vector<Element> rows;
    bool showDividers = true;
    Style style {};

    Element body() const;
};

} // namespace flux
