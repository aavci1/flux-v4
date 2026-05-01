#pragma once

#include "../Common.hpp"
#include "../Helpers.hpp"
#include "../components/ScaleStrip.hpp"

namespace car {

struct ZonePanel : ViewModifiers<ZonePanel> {
    std::string label;
    Reactive::Bindable<float> temp{21.5f};
    std::function<void(float)> onChange;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Reactive::Bindable<float> pct{[temp = temp] { return (temp.evaluate() - 16.f) / 12.f; }};
        Element body = VStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                Text{.text = label, .font = Font::caption2(), .color = Color::tertiary()},
                HStack{
                    .spacing = theme().space1,
                    .alignment = Alignment::End,
                    .children = children(
                        Text {
                            .text = [temp = temp] { return format1(temp.evaluate()); },
                            .font = Font {
                                .size = 64.f,
                                .weight = 300.f
                            },
                            .color = Color::primary()
                        },
                        Text {
                            .text = "°C",
                            .font = Font {
                                .size = 32.f,
                                .weight = 300.f
                            },
                            .color = Color::secondary()
                        }
                            .padding(0.f, 0.f, theme().space2, 0.f)
                    )
                }
                    .padding(theme().space2, 0.f, 0.f, 0.f),
                HStack {
                    .spacing = theme().space5,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        ScaleStrip {
                            .pct = pct
                        },
                        VStack {
                            .spacing = theme().space2,
                            .alignment = Alignment::Stretch,
                            .justifyContent = JustifyContent::End,
                            .children = children(
                                IconButton {
                                    .icon = IconName::Add,
                                    .style = IconButton::Style {
                                        .size = 48.f
                                    },
                                    .onTap = [temp = temp, onChange = onChange] { if (onChange) onChange(std::min(28.f, temp.evaluate() + 0.5f)); }
                                },
                                IconButton {
                                    .icon = IconName::Remove,
                                    .style = IconButton::Style {
                                        .size = 48.f
                                    },
                                    .onTap = [temp = temp, onChange = onChange] { if (onChange) onChange(std::max(16.f, temp.evaluate() - 0.5f)); }
                                }
                            )
                        }
                            .flex(1.f, 1.f, 0.f)
                    ),
                }
                .padding(theme().space6, 0.f, 0.f, 0.f)
                .flex(1.f, 1.f, 0.f),
                Grid {
                    .columns = 2,
                    .horizontalSpacing = theme().space2,
                    .verticalSpacing = theme().space2,
                    .children = children(
                        Button {
                            .label = std::string{"Heat seat"},
                            .variant = ButtonVariant::Secondary
                        },
                        Button {
                            .label = std::string{"Cool seat"},
                            .variant = ButtonVariant::Secondary
                        }
                    )
                }
                .padding(18.f, 0.f, 0.f, 0.f)
            ),
        };
        return makeCard(body, 32.f);
    }
};

} // namespace car
