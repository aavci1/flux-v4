#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"
#include "../components/Avatar.hpp"
#include "../components/Tab.hpp"

namespace car {

struct PhoneScreen : ViewModifiers<PhoneScreen> {
    Reactive::Signal<State> state;
    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        std::vector<Recent> recents = state().recents;
        std::vector<Element> rows;
        for (Recent const& r : recents) {
            rows.push_back(HStack {
                .spacing = theme().space3,
                .alignment = Alignment::Center,
                .children = children(
                    Avatar{.initials = r.initials, .sizeP = 36.f},
                    VStack {
                        .spacing = 0.f,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text{.text = r.name, .font = Font{.size = 13.f, .weight = 500.f}, .color = r.missed ? Color::danger() : Color::primary()},
                            HStack{.spacing = theme().space1, .alignment = Alignment::Center,
                                   .children = children(Icon{.name = r.outgoing ? IconName::CallMade : IconName::CallReceived, .size = 11.f, .color = r.missed ? Color::danger() : Color::tertiary()},
                                                        Text{.text = r.when, .font = Font::caption(), .color = Color::tertiary()})}
                        ),
                    }.flex(1.f, 1.f, 0.f),
                    Icon{.name = IconName::Info, .size = 16.f, .color = Color::tertiary()}
                ),
            }.padding(14.f, theme().space5, 14.f, theme().space5));
            rows.push_back(Divider{.orientation = Divider::Orientation::Horizontal});
        }

        Recent top = recents.empty() ? Recent{"--", "-", "", false, false} : recents.front();

        Element leftCard = Card {
            .child = VStack {
                .spacing = 0.f,
                .alignment = Alignment::Stretch,
                .children = children(
                    HStack {
                        .spacing = theme().space2,
                        .alignment = Alignment::Center,
                           .children = children(
                            Tab {
                                .label = "Recents",
                                .active = true
                            },
                            Tab {
                                .label = "Contacts"
                            },
                            Tab {
                                .label = "Keypad"
                            }
                        )
                    }
                        .padding(theme().space4, theme().space5, theme().space4, theme().space5),
                    Divider {
                        .orientation = Divider::Orientation::Horizontal
                    },
                    ScrollView {
                        .axis = ScrollAxis::Vertical,
                        .children = children(
                            VStack {
                                .spacing = 0.f,
                                .alignment = Alignment::Stretch,
                                .children = std::move(rows)
                            }
                        )
                    }
                    .flex(1.f, 1.f, 0.f)
                ),
            },
            .style = Card::Style{
                .padding = 0.f,
                .cornerRadius = 10.f,
                .backgroundColor = Color::elevatedBackground(),
                .borderColor = Color::separator()
            },
        };

        Element rightCard = makeCard(
            VStack {
                .spacing = theme().space4,
                .alignment = Alignment::Center,
                .justifyContent = JustifyContent::Center,
                .children = children(
                    Avatar {
                        .initials = top.initials,
                        .sizeP = 120.f,
                        .useAccent = true
                    },
                    Text {
                        .text = top.name,
                        .font = Font {.size = 28.f, .weight = 400.f},
                        .color = Color::primary()
                    }
                        .padding(20.f, 0.f, 0.f, 0.f),
                    Text {
                        .text = "Mobile / +49 173 555 1842",
                        .font = Font::subheadline(),
                        .color = Color::secondary()
                    },
                    HStack {
                        .spacing = theme().space3,
                        .alignment = Alignment::Center,
                        .justifyContent = JustifyContent::Center,
                        .children = children(
                            Button {
                                .label = std::string {"Call"},
                                .variant = ButtonVariant::Primary
                            },
                            Button {
                                .label = std::string {"Message"},
                                .variant = ButtonVariant::Secondary
                            },
                            Button {
                                .label = std::string {"FaceTime"},
                                .variant = ButtonVariant::Secondary
                            }
                        )
                    }
                        .padding(28.f, 0.f, 0.f, 0.f)
                ),
            },
            32.f
        );

        return Grid {
            .columns = 5,
            .horizontalSpacing = theme().space4,
            .verticalSpacing = theme().space4,
            .horizontalAlignment = Alignment::Stretch,
            .verticalAlignment = Alignment::Stretch,
            .children = children(
                std::move(leftCard).colSpan(2u),
                std::move(rightCard).colSpan(3u)
            ),
        }
            .padding(theme().space4);
    }
};

} // namespace car
