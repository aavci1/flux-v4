#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"
#include "../components/Avatar.hpp"
#include "../components/CardHeader.hpp"

namespace car {

struct PhoneCard : ViewModifiers<PhoneCard> {
    Reactive::Signal<State> state;
    std::function<void(Screen)> onNavigate;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        std::vector<Element> rows;
        std::vector<Recent> recents = state().recents;
        rows.reserve(3);
        for (std::size_t i = 0; i < std::min<std::size_t>(3, recents.size()); ++i) {
            Recent r = recents[i];
            rows.push_back(HStack {
                .spacing = theme().space2,
                .alignment = Alignment::Center,
                .children = children(
                    Avatar{.initials = r.initials, .sizeP = 32.f},
                    VStack {
                        .spacing = 0.f,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text{.text = r.name, .font = Font{.size = 13.f, .weight = 500.f}, .color = Color::primary(), .wrapping = TextWrapping::NoWrap, .maxLines = 1},
                            HStack {
                                .spacing = theme().space1,
                                .alignment = Alignment::Center,
                                .children = children(
                                    Icon{.name = r.outgoing ? IconName::CallMade : IconName::CallReceived,
                                         .size = 11.f, .color = r.missed ? Color::danger() : Color::tertiary()},
                                    Text{.text = r.when, .font = Font::caption(), .color = Color::tertiary()}
                                ),
                            }
                        ),
                    }.flex(1.f, 1.f, 0.f)
                ),
            });
        }
        Element body = VStack {
            .spacing = theme().space2,
            .alignment = Alignment::Stretch,
            .children = children(
                CardHeader{.icon = IconName::Call, .title = "RECENTS", .trailingChevron = true},
                VStack{.spacing = theme().space2, .alignment = Alignment::Stretch, .children = std::move(rows)}.flex(1.f, 1.f, 0.f)
            ),
        };
        return makeCard(body, 20.f)
            .cursor(Cursor::Hand)
            .onTap([onNavigate = onNavigate] { if (onNavigate) onNavigate(Screen::Phone); });
    }
};

} // namespace car
