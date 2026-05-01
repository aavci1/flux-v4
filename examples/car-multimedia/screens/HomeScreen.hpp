#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../cards/DriverAssistCard.hpp"
#include "../cards/MapPreviewCard.hpp"
#include "../cards/NowPlayingCard.hpp"
#include "../cards/PhoneCard.hpp"
#include "../cards/VehicleStatusCard.hpp"

namespace car {

struct HomeScreen : ViewModifiers<HomeScreen> {
    Reactive::Signal<State> state;
    std::function<void(Screen)> onNavigate;
    std::function<void(std::string)> onAction;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        return Grid {
            .columns = 4,
            .horizontalSpacing = theme().space4,
            .verticalSpacing = theme().space4,
            .horizontalAlignment = Alignment::Stretch,
            .verticalAlignment = Alignment::Stretch,
            .children = children(
                MapPreviewCard{.state = state, .onNavigate = onNavigate}.colSpan(2u).rowSpan(2u),
                NowPlayingCard{.state = state, .onAction = onAction},
                VehicleStatusCard{.state = state, .onNavigate = onNavigate},
                PhoneCard{.state = state, .onNavigate = onNavigate},
                DriverAssistCard{.state = state, .onAction = onAction}
            ),
        }
            .padding(theme().space4)
            .flex(1.f, 1.f, 0.f);
    }
};

} // namespace car
