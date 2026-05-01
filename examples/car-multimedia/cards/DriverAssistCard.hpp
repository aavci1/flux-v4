#pragma once

#include "../AppState.hpp"
#include "../Common.hpp"
#include "../Helpers.hpp"
#include "../components/CardHeader.hpp"
#include "AssistTile.hpp"

namespace car {

struct DriverAssistCard : ViewModifiers<DriverAssistCard> {
    Reactive::Signal<State> state;
    std::function<void(std::string)> onAction;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Element body = VStack {
            .spacing = theme().space2,
            .alignment = Alignment::Stretch,
            .children = children(
                CardHeader {
                    .icon = IconName::AdaptiveAudioMic,
                    .title = "DRIVER ASSIST"
                },
                Grid {
                    .columns = 2,
                    .horizontalSpacing = theme().space2,
                    .verticalSpacing = theme().space2,
                    .children = children(
                        AssistTile {
                            .icon = IconName::Speed,
                            .label = "Cruise",
                            .active = [s = state] { return s().assist.cruise; },
                            .value = "120 km/h",
                            .onTap = [onAction = onAction] { if (onAction) onAction("cruise"); },
                        },
                        AssistTile {
                            .icon = IconName::Forklift,
                            .label = "Lane Assist",
                            .active = [s = state] { return s().assist.lane; },
                            .onTap = [onAction = onAction] { if (onAction) onAction("lane"); },
                        },
                        AssistTile {
                            .icon = IconName::LocalParking,
                            .label = "Park Assist",
                            .active = [s = state] { return s().assist.park; },
                            .onTap = [onAction = onAction] { if (onAction) onAction("park"); },
                        },
                        AssistTile {
                            .icon = IconName::Visibility,
                            .label = "Night Vision",
                            .active = [s = state] { return s().assist.night; },
                            .onTap = [onAction = onAction] { if (onAction) onAction("night"); },
                        }
                    ),
                }
                    .flex(1.f, 1.f, 0.f)
            ),
        };
        return makeCard(body, 20.f);
    }
};

} // namespace car
