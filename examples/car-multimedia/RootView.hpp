#pragma once

#include "AppState.hpp"
#include "Common.hpp"
#include "Constants.hpp"
#include "Helpers.hpp"
#include "chrome/ClimateStrip.hpp"
#include "chrome/NavRail.hpp"
#include "chrome/TopBar.hpp"
#include "screens/ScreenHost.hpp"

namespace car {

struct RootView : ViewModifiers<RootView> {
    auto body() const {
        auto state = useState<State>(makeInitialState());

        auto setActive = [state](Screen screen) {
            mutate(state, [screen](State &s) { s.active = screen; });
        };

        auto setClimate = [state](ClimateState climate) {
            mutate(state, [climate = std::move(climate)](State &s) { s.climate = climate; });
        };

        auto setVehicleControls = [state](VehicleControls controls) {
            mutate(state, [controls = std::move(controls)](State &s) { s.vehicleControls = controls; });
        };

        auto onAction = [state](std::string action) {
            mutate(state, [action = std::move(action)](State &s) {
                if (action == "toggle") {
                    s.music.playing = !s.music.playing;
                    return;
                }

                if (action == "next" || action == "prev") {
                    if (s.queue.empty())
                        return;
                    int idx = 0;
                    for (std::size_t i = 0; i < s.queue.size(); ++i) {
                        if (s.queue[i].title == s.music.title) {
                            idx = static_cast<int>(i);
                            break;
                        }
                    }
                    int n = static_cast<int>(s.queue.size());
                    int next = action == "next" ? (idx + 1) % n : (idx - 1 + n) % n;
                    s.music.title = s.queue[static_cast<std::size_t>(next)].title;
                    s.music.artist = s.queue[static_cast<std::size_t>(next)].artist;
                    s.music.progress = 0.f;
                    s.music.playing = true;
                    return;
                }

                if (action == "cruise")
                    s.assist.cruise = !s.assist.cruise;
                else if (action == "lane")
                    s.assist.lane = !s.assist.lane;
                else if (action == "park")
                    s.assist.park = !s.assist.park;
                else if (action == "night")
                    s.assist.night = !s.assist.night;
            });
        };

        return VStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                TopBar {.state = state}.flex(0.f, 0.f, 54.f),
                Divider {.orientation = Divider::Orientation::Horizontal},
                HStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        NavRail {.state = state, .onChange = setActive}.flex(0.f, 0.f),
                        Divider {.orientation = Divider::Orientation::Vertical},
                        ScreenHost {
                            .state = state,
                            .setActive = setActive,
                            .onAction = onAction,
                            .setClimate = setClimate,
                            .setVehicleControls = setVehicleControls
                        }
                            .flex(1.f, 1.f, 0.f)
                            .clipContent(true)
                    ),
                }
                    .flex(1.f, 1.f),
                Divider {.orientation = Divider::Orientation::Horizontal},
                ClimateStrip {.state = state, .onChange = setClimate, .onOpen = [setActive] { setActive(Screen::Climate); }}.flex(0.f, 0.f, 64.f)
            ),
        }
            .fill(Color::windowBackground());
    }
};

} // namespace car
