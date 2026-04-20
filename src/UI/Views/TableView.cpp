#include <algorithm>
#include <numeric>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/UI/Views/TableView.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

namespace flux {

namespace {

struct TableColumnLayout {
    float width = 0.f;
    float flexGrow = 0.f;
};

struct TableLayoutContext {
    std::vector<TableColumnLayout> columns;
};

struct TableColumnIndex {
    std::size_t value = 0;
};

TableRow::Style resolveRowStyle(TableRow::Style const &style, Theme const &theme) {
    return TableRow::Style {
        .paddingH = resolveFloat(style.paddingH, theme.space4),
        .paddingV = resolveFloat(style.paddingV, theme.space2),
        .spacing = resolveFloat(style.spacing, theme.space2),
        .backgroundColor = resolveColor(style.backgroundColor, theme.elevatedBackgroundColor, theme),
        .hoverBackgroundColor = resolveColor(style.hoverBackgroundColor, theme.rowHoverBackgroundColor, theme),
        .selectedBackgroundColor = resolveColor(style.selectedBackgroundColor, theme.selectedContentBackgroundColor, theme),
    };
}

TableView::Style resolveTableStyle(TableView::Style const &style, Theme const &theme) {
    return TableView::Style {
        .dividerInsetH = resolveFloat(style.dividerInsetH, theme.space4),
        .backgroundColor = resolveColor(style.backgroundColor, theme.windowBackgroundColor, theme),
        .dividerColor = resolveColor(style.dividerColor, theme.separatorColor, theme),
    };
}

Alignment resolveHorizontalAlignment(HorizontalAlignment alignment) {
    switch (alignment) {
        case HorizontalAlignment::Leading:
            return Alignment::Start;
        case HorizontalAlignment::Center:
            return Alignment::Center;
        case HorizontalAlignment::Trailing:
            return Alignment::End;
    }
    return Alignment::Start;
}

Element divider(Color color, float insetH) {
    return Rectangle {}
        .size(0.f, 1.f)
        .fill(FillStyle::solid(color))
        .padding(0.f, insetH, 0.f, insetH);
}

bool isSortable(TableColumn const &column) {
    return column.sort && static_cast<bool>(column.sort->less);
}

TableColumn::Sort const *activeSort(std::vector<TableColumn> const &columns, int activeColumn) {
    if (activeColumn < 0 || static_cast<std::size_t>(activeColumn) >= columns.size()) {
        return nullptr;
    }
    TableColumn const &column = columns[static_cast<std::size_t>(activeColumn)];
    if (!isSortable(column)) {
        return nullptr;
    }
    return &*column.sort;
}

std::any const *findSortValue(TableView::Item const &item, std::size_t columnIndex) {
    for (TableView::SortValue const &entry : item.sortValues) {
        if (entry.column == columnIndex) {
            return &entry.value;
        }
    }
    return nullptr;
}

bool shouldPlaceBefore(TableView::Item const &lhs, TableView::Item const &rhs, std::size_t columnIndex,
                       TableColumn::Sort const &sort, bool ascending) {
    std::any const *lhsValue = findSortValue(lhs, columnIndex);
    std::any const *rhsValue = findSortValue(rhs, columnIndex);

    bool const lhsPresent = lhsValue && lhsValue->has_value();
    bool const rhsPresent = rhsValue && rhsValue->has_value();
    if (lhsPresent != rhsPresent) {
        return lhsPresent;
    }
    if (!lhsPresent) {
        return false;
    }

    bool const lhsBeforeRhs = sort.less(*lhsValue, *rhsValue);
    bool const rhsBeforeLhs = sort.less(*rhsValue, *lhsValue);
    if (lhsBeforeRhs == rhsBeforeLhs) {
        return false;
    }
    return ascending ? lhsBeforeRhs : rhsBeforeLhs;
}

Element sortIndicator(bool active, bool ascending, Theme const &theme) {
    return Icon {
        .name = active ? (ascending ? IconName::ArrowUpward : IconName::ArrowDownward) : IconName::Sort,
        .size = 16.f,
        .color = active ? theme.accentColor : theme.tertiaryLabelColor,
    };
}

Element makeSortableHeaderCell(Element cell, std::size_t columnIndex, TableColumn::Sort sort, bool active,
                               bool ascending, Theme const &theme, State<int> sortColumn,
                               State<bool> sortAscending) {
    auto activateSort = [columnIndex, sort = std::move(sort), sortColumn, sortAscending] {
        int const requested = static_cast<int>(columnIndex);
        if (*sortColumn == requested) {
            sortAscending = !*sortAscending;
            return;
        }
        sortColumn = requested;
        sortAscending = sort.initialAscending;
    };
    auto handleKey = [activateSort](KeyCode key, Modifiers) {
        if (key == keys::Return || key == keys::Space) {
            activateSort();
        }
    };

    if (cell.is<TableCell>()) {
        TableCell headerCell = cell.as<TableCell>();
        headerCell.content = HStack {
            .spacing = theme.space1,
            .alignment = Alignment::Center,
            .children = children(std::move(headerCell.content), sortIndicator(active, ascending, theme)),
        };
        cell = Element {std::move(headerCell)};
    }

    return std::move(cell)
        .cursor(Cursor::Hand)
        .focusable(true)
        .onKeyDown(handleKey)
        .onTap(activateSort);
}

Element decorateHeader(Element header, std::vector<TableColumn> const &columns, int activeColumn, bool ascending,
                       Theme const &theme, State<int> sortColumn, State<bool> sortAscending) {
    if (!header.is<TableRow>()) {
        return header;
    }

    TableRow row = header.as<TableRow>();
    for (std::size_t i = 0; i < row.cells.size() && i < columns.size(); ++i) {
        if (!isSortable(columns[i])) {
            continue;
        }
        row.cells[i] =
            makeSortableHeaderCell(std::move(row.cells[i]), i, *columns[i].sort, activeColumn == static_cast<int>(i),
                                   ascending, theme, sortColumn, sortAscending);
    }
    return Element {std::move(row)};
}

} // namespace

Element TableCell::body() const {
    TableLayoutContext const &table = useEnvironment<TableLayoutContext>();
    TableColumnIndex const &index = useEnvironment<TableColumnIndex>();

    float resolvedWidth = style.width > 0.f ? style.width : 0.f;
    bool usesTableFlex = false;
    if (index.value < table.columns.size()) {
        TableColumnLayout const &column = table.columns[index.value];
        if (column.width > 0.f) {
            resolvedWidth = column.width;
        } else if (column.flexGrow > 0.f) {
            usesTableFlex = true;
        }
    }

    Element cellContent = content;
    if (resolvedWidth <= 0.f && !usesTableFlex && style.alignment == HorizontalAlignment::Leading) {
        return cellContent;
    }

    Element aligned = ZStack {
        .horizontalAlignment = resolveHorizontalAlignment(style.alignment),
        .verticalAlignment = Alignment::Center,
        .children = children(std::move(cellContent)),
    };
    if (resolvedWidth > 0.f) {
        return std::move(aligned).width(resolvedWidth);
    }
    return aligned;
}

Element TableRow::body() const {
    Theme const &theme = useEnvironment<Theme>();
    TableRow::Style const resolved = resolveRowStyle(style, theme);
    TableLayoutContext const &table = useEnvironment<TableLayoutContext>();
    bool const hovered = useHover();
    bool const pressed = usePress();
    bool const interactive = !disabled && static_cast<bool>(onTap);

    Color const fill = selected ? resolved.selectedBackgroundColor :
                      pressed && interactive ? resolved.hoverBackgroundColor :
                      hovered ? resolved.hoverBackgroundColor :
                                resolved.backgroundColor;

    auto handleTap = [onTap = onTap, disabled = disabled] {
        if (!disabled && onTap) {
            onTap();
        }
    };
    auto handleKey = [handleTap](KeyCode key, Modifiers) {
        if (key == keys::Return || key == keys::Space) {
            handleTap();
        }
    };

    std::vector<Element> rowCells;
    rowCells.reserve(cells.size());
    for (std::size_t i = 0; i < cells.size(); ++i) {
        Element cell = cells[i];
        cell = std::move(cell).environment(TableColumnIndex {.value = i});
        if (i < table.columns.size()) {
            TableColumnLayout const &column = table.columns[i];
            if (column.width <= 0.f && column.flexGrow > 0.f) {
                cell = std::move(cell).flex(column.flexGrow, 1.f, 0.f);
            }
        }
        rowCells.push_back(std::move(cell));
    }

    std::vector<Element> childrenList;
    childrenList.reserve(detail ? 2u : 1u);
    childrenList.push_back(
        HStack {
            .spacing = resolved.spacing,
            .alignment = Alignment::Center,
            .children = std::move(rowCells),
        }
            .padding(resolved.paddingV, resolved.paddingH, resolved.paddingV, resolved.paddingH)
            .fill(FillStyle::solid(fill))
    );
    if (detail) {
        childrenList.push_back(*detail);
    }

    return VStack {
        .spacing = 0.f,
        .alignment = Alignment::Stretch,
        .children = std::move(childrenList),
    }
        .cursor(interactive ? Cursor::Hand : Cursor::Arrow)
        .focusable(interactive)
        .onKeyDown(interactive ? std::function<void(KeyCode, Modifiers)> {handleKey}
                               : std::function<void(KeyCode, Modifiers)> {})
        .onTap(interactive ? std::function<void()> {handleTap} : std::function<void()> {});
}

Element TableView::body() const {
    Theme const &theme = useEnvironment<Theme>();
    TableView::Style const resolved = resolveTableStyle(style, theme);
    State<int> const sortColumn = useState<int>(-1);
    State<bool> const sortAscending = useState<bool>(true);
    TableLayoutContext layout {};
    layout.columns.reserve(columns.size());
    for (TableColumn const &column : columns) {
        layout.columns.push_back(TableColumnLayout {
            .width = column.width > 0.f ? column.width : 0.f,
            .flexGrow = column.width > 0.f ? 0.f : std::max(0.f, column.flexGrow),
        });
    }

    int const activeColumn = activeSort(columns, *sortColumn) ? *sortColumn : -1;
    std::vector<std::size_t> itemOrder(items.size());
    std::iota(itemOrder.begin(), itemOrder.end(), 0u);
    if (TableColumn::Sort const *sort = activeSort(columns, activeColumn)) {
        std::stable_sort(itemOrder.begin(), itemOrder.end(), [&](std::size_t lhsIndex, std::size_t rhsIndex) {
            return shouldPlaceBefore(items[lhsIndex], items[rhsIndex], static_cast<std::size_t>(activeColumn), *sort,
                                     *sortAscending);
        });
    }

    std::size_t const totalRows = itemOrder.size() + rows.size();
    std::vector<Element> rowChildren;
    rowChildren.reserve(showDividers && totalRows > 0 ? totalRows * 2u - 1u : totalRows);
    std::size_t renderedRowCount = 0;
    for (std::size_t index : itemOrder) {
        if (showDividers && renderedRowCount > 0) {
            rowChildren.push_back(divider(resolved.dividerColor, resolved.dividerInsetH));
        }
        Element row = items[index].row;
        rowChildren.push_back(std::move(row).environment(layout));
        ++renderedRowCount;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (showDividers && renderedRowCount > 0) {
            rowChildren.push_back(divider(resolved.dividerColor, resolved.dividerInsetH));
        }
        Element row = rows[i];
        rowChildren.push_back(std::move(row).environment(layout));
        ++renderedRowCount;
    }

    Element bodyContent = VStack {
        .spacing = 0.f,
        .alignment = Alignment::Stretch,
        .children = std::move(rowChildren),
    };

    Element bodyElement = scrollBody
                              ? Element {ScrollView {
                                    .axis = ScrollAxis::Vertical,
                                    .children = children(std::move(bodyContent)),
                                }}
                                    .flex(1.f, 1.f, 0.f)
                              : std::move(bodyContent);

    std::vector<Element> childrenList;
    childrenList.reserve(header ? 3u : 1u);
    if (header) {
        Element headerRow = *header;
        if (!items.empty()) {
            headerRow =
                decorateHeader(std::move(headerRow), columns, activeColumn, *sortAscending, theme, sortColumn, sortAscending);
        }
        childrenList.push_back(std::move(headerRow).environment(layout));
        if (showDividers && totalRows > 0) {
            childrenList.push_back(divider(resolved.dividerColor, resolved.dividerInsetH));
        }
    }
    childrenList.push_back(std::move(bodyElement));

    return VStack {
        .spacing = 0.f,
        .alignment = Alignment::Stretch,
        .children = std::move(childrenList),
    }
        .fill(FillStyle::solid(resolved.backgroundColor));
}

} // namespace flux
