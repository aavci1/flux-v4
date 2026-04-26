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

        bool operator==(Sort const& other) const {
            return initialAscending == other.initialAscending &&
                   static_cast<bool>(less) == static_cast<bool>(other.less);
        }

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

    bool operator==(TableColumn const& other) const = default;
};

struct TableCell : ViewModifiers<TableCell> {
    struct Style {
        float width = kFloatFromTheme;
        HorizontalAlignment alignment = HorizontalAlignment::Leading;

        bool operator==(Style const& other) const = default;
    };

    Element content;
    Style style {};

    bool operator==(TableCell const& other) const {
        return content.structuralEquals(other.content) && style == other.style;
    }

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

        bool operator==(Style const& other) const = default;
    };

    std::vector<Element> cells;
    std::optional<Element> detail {};
    bool selected = false;
    bool disabled = false;
    Style style {};
    std::function<void()> onTap;

    bool operator==(TableRow const& other) const {
        return elementsStructurallyEqual(cells, other.cells) && detail.has_value() == other.detail.has_value() &&
               (!detail || detail->structuralEquals(*other.detail)) && selected == other.selected &&
               disabled == other.disabled && style == other.style;
    }

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

        bool operator==(SortValue const& other) const {
            // std::any is not structurally comparable. Column identity and value presence are enough
            // for retained subtree safety; actual sorting is recomputed inside TableView::body().
            return column == other.column && value.has_value() == other.value.has_value();
        }
    };

    struct Item {
        Element row;
        std::vector<SortValue> sortValues;

        bool operator==(Item const& other) const {
            return row.structuralEquals(other.row) && sortValues == other.sortValues;
        }
    };

    struct Style {
        float dividerInsetH = kFloatFromTheme;
        Color backgroundColor = Color::theme();
        Color dividerColor = Color::theme();

        bool operator==(Style const& other) const = default;
    };

    std::optional<Element> header {};
    std::vector<Item> items;
    std::vector<Element> rows;
    std::vector<TableColumn> columns;
    bool showDividers = true;
    bool scrollBody = true;
    Style style {};

    bool operator==(TableView const& other) const {
        return header.has_value() == other.header.has_value() &&
               (!header || header->structuralEquals(*other.header)) &&
               items == other.items &&
               elementsStructurallyEqual(rows, other.rows) &&
               columns == other.columns && showDividers == other.showDividers &&
               scrollBody == other.scrollBody && style == other.style;
    }

    Element body() const;
};

} // namespace flux
