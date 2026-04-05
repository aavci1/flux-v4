#pragma once

#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

using namespace flux;

struct ChatListRow : ViewModifiers<ChatListRow> {
    size_t index = 0;
    std::string title;
    bool selected = false;
    std::function<void(size_t)> onSelect;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        auto isHovered = useHover();

        FillStyle const rowFill = selected
            ? FillStyle::solid(Color::hex(0xDCE8F5))
            : isHovered ? FillStyle::solid(Color::hex(0xEBEDEF)) : FillStyle::none();

        return HStack {
            .spacing = 12.f,
            .alignment = Alignment::Center,
            .children = children(
                Text {
                    .text = title,
                    .style = theme.typeSubtitle,
                    .color = theme.colorTextPrimary,
                    .verticalAlignment = VerticalAlignment::Center,
                    .wrapping = TextWrapping::NoWrap,
                },
                Spacer {},
                Icon {
                    .name = IconName::MoreHoriz,
                    .size = theme.typeSubtitle.size,
                    .weight = 300.f,
                    .color = isHovered ? theme.colorTextPrimary : Colors::transparent,
                }
            )
        }
        .fill(rowFill)
        .cursor(Cursor::Hand)
        .padding(8.f)
        .onTap([onSelect = onSelect, idx = index]() {
            if (onSelect) {
                onSelect(idx);
            }
        });
    }
};