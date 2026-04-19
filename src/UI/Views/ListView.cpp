#include <Flux/Core/KeyCodes.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/ListView.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/VStack.hpp>

namespace flux {

namespace {

ListRow::Style resolveRowStyle(ListRow::Style const &style, Theme const &theme) {
    return ListRow::Style {
        .paddingH = resolveFloat(style.paddingH, theme.space4),
        .paddingV = resolveFloat(style.paddingV, theme.space3),
    };
}

ListView::Style resolveListStyle(ListView::Style const &style, Theme const &theme) {
    return ListView::Style {
        .dividerInsetH = resolveFloat(style.dividerInsetH, theme.space4),
    };
}

} // namespace

Element ListRow::body() const {
    Theme const &theme = useEnvironment<Theme>();
    ListRow::Style const resolved = resolveRowStyle(style, theme);
    bool const hovered = useHover();
    bool const pressed = usePress();
    bool const isDisabled = disabled;

    Color const fill = selected ? theme.selectedContentBackgroundColor : pressed ? theme.rowHoverBackgroundColor :
                                                        hovered     ? theme.hoveredControlBackgroundColor :
                                                                      Colors::transparent;

    auto handleTap = [onTap = onTap, isDisabled]() {
        if (!isDisabled && onTap) {
            onTap();
        }
    };
    auto handleKey = [handleTap](KeyCode key, Modifiers) {
        if (key == keys::Return || key == keys::Space) {
            handleTap();
        }
    };

    Element rowContent = content;
    return std::move(rowContent)
        .padding(resolved.paddingV, resolved.paddingH, resolved.paddingV, resolved.paddingH)
        .fill(FillStyle::solid(fill))
        .cursor(isDisabled ? Cursor::Arrow : Cursor::Hand)
        .focusable(!isDisabled)
        .onKeyDown(isDisabled ? std::function<void(KeyCode, Modifiers)>{} : std::function<void(KeyCode, Modifiers)>{handleKey})
        .onTap(isDisabled ? std::function<void()>{} : std::function<void()>{handleTap});
}

Element ListView::body() const {
    Theme const &theme = useEnvironment<Theme>();
    ListView::Style const resolved = resolveListStyle(style, theme);

    std::vector<Element> childrenList;
    childrenList.reserve(showDividers && !rows.empty() ? rows.size() * 2 - 1 : rows.size());

    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (showDividers && i > 0) {
            childrenList.emplace_back(
                Rectangle {}
                    .size(0.f, 1.f)
                    .fill(FillStyle::solid(Color::separator()))
                    .padding(0.f, resolved.dividerInsetH, 0.f, resolved.dividerInsetH)
            );
        }
        childrenList.push_back(rows[i]);
    }

    return ScrollView {
        .axis = ScrollAxis::Vertical,
        .children = children(
            VStack {
                .spacing = 0.f,
                .alignment = Alignment::Stretch,
                .children = std::move(childrenList),
            }
        ),
    };
}

} // namespace flux
