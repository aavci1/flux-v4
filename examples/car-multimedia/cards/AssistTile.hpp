#pragma once

#include "../Common.hpp"

namespace car {

struct AssistTile : ViewModifiers<AssistTile> {
    IconName icon = IconName::Info;
    std::string label;
    Reactive::Bindable<bool> active {false};
    std::optional<std::string> value;
    std::function<void()> onTap;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();

        auto motion = [theme] { return Transition::ease(theme().durationFast); };
        auto bg = useAnimated([active = active, theme] {
            Theme const &t = theme();
            return active.evaluate() ? t.selectedContentBackgroundColor : t.controlBackgroundColor;
        }, motion);
        auto border = useAnimated([active = active, theme] {
            Theme const &t = theme();
            return active.evaluate() ? t.accentColor : t.separatorColor;
        }, motion);
        auto labelColor = useAnimated([active = active, theme] {
            Theme const &t = theme();
            return active.evaluate() ? t.accentColor : t.labelColor;
        }, motion);

        std::vector<Element> children;
        children.push_back(
            Icon {
                .name = icon,
                .size = 18.f,
                .weight = 500.f,
                .color = labelColor
            }
        );

        children.push_back(
            Text {
                .text = label,
                .font = Font {
                    .size = 11.f,
                    .weight = 500.f
                },
                .color = labelColor
            }
        );

        if (value) {
            children.push_back(
                Text {
                    .text = *value,
                    .font = Font::caption2(),
                    .color = Color::tertiary()
                }
            );
        }

        return VStack {
            .spacing = theme().space1,
            .alignment = Alignment::Start,
            .children = std::move(children)
        }
            .padding(theme().space2, theme().space3, theme().space2, theme().space3)
            .fill(bg)
            .stroke(border, 1.f)
            .cornerRadius(theme().radiusLarge)
            .cursor(Cursor::Hand)
            .onTap([onTap = onTap] {
                if (onTap) {
                    onTap();
                }
            });
    }
};

} // namespace car
