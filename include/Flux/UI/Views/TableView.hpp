#pragma once

/// \file Flux/UI/Views/TableView.hpp
///
/// Part of the Flux public API.

#include <Flux/Core/Types.hpp>
#include <Flux/UI/Element.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/Theme.hpp>

#include <any>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace flux {

struct TableColumn {
    struct Sort {
        bool initialAscending = true;
        std::function<bool(std::any const &, std::any const &)> less;

        template<typename T, typename Compare = std::less<>>
        static Sort by(Compare compare = Compare {}, bool initialAscending = true) {
            return Sort {
                .initialAscending = initialAscending,
                .less = [compare = std::move(compare)](std::any const &lhs, std::any const &rhs) {
                    return std::invoke(compare, std::any_cast<T const &>(lhs), std::any_cast<T const &>(rhs));
                },
            };
        }

        template<typename T>
        static Sort ascending() {
            return by<T>(std::less<> {}, true);
        }

        template<typename T>
        static Sort descending() {
            return by<T>(std::less<> {}, false);
        }
    };

    float width = kFloatFromTheme;
    float flexGrow = 0.f;
    std::optional<Sort> sort {};
};

struct TableCell : ViewModifiers<TableCell> {
    struct Style {
        float width = kFloatFromTheme;
        HorizontalAlignment alignment = HorizontalAlignment::Leading;
    };

    Element content;
    Style style {};

    Element body() const;
};

struct TableRow : ViewModifiers<TableRow> {
    struct Style {
        float paddingH = kFloatFromTheme;
        float paddingV = kFloatFromTheme;
        float spacing = kFloatFromTheme;
        Color backgroundColor = Color::theme();
        Color hoverBackgroundColor = Color::theme();
        Color selectedBackgroundColor = Color::theme();
    };

    std::vector<Element> cells;
    std::optional<Element> detail {};
    bool selected = false;
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    Element body() const;
};

struct TableView : ViewModifiers<TableView> {
    struct SortValue {
        std::size_t column = 0;
        std::any value {};

        SortValue() = default;
        SortValue(SortValue const &) = default;
        SortValue(SortValue &&) noexcept = default;
        SortValue &operator=(SortValue const &) = default;
        SortValue &operator=(SortValue &&) noexcept = default;

        template<typename T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, SortValue>>>
        SortValue(std::size_t columnIndex, T &&sortValue)
            : column(columnIndex)
            , value(std::forward<T>(sortValue)) {}
    };

    struct Item {
        Element row;
        std::vector<SortValue> sortValues;
    };

    struct Style {
        float dividerInsetH = kFloatFromTheme;
        Color backgroundColor = Color::theme();
        Color dividerColor = Color::theme();
    };

    std::optional<Element> header {};
    std::vector<Item> items;
    std::vector<Element> rows;
    std::vector<TableColumn> columns;
    bool showDividers = true;
    bool scrollBody = true;
    Style style {};

    Element body() const;
};

} // namespace flux
