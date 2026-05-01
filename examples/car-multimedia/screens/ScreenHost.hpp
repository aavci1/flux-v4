#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "ClimateScreen.hpp"
#include "HomeScreen.hpp"
#include "MapScreen.hpp"
#include "MusicScreen.hpp"
#include "PhoneScreen.hpp"
#include "SettingsScreen.hpp"
#include "VehicleScreen.hpp"

namespace car {

struct ScreenHost : ViewModifiers<ScreenHost> {
    Reactive::Signal<State> state;
    std::function<void(Screen)> setActive;
    std::function<void(std::string)> onAction;
    std::function<void(ClimateState)> setClimate;
    std::function<void(VehicleControls)> setVehicleControls;

    auto body() const {
        auto stateSignal = state;
        auto setActiveFn = setActive;
        auto onActionFn = onAction;
        auto setClimateFn = setClimate;
        auto setVehicleControlsFn = setVehicleControls;

        return Switch(
            [s = stateSignal] { return s().active; },
            {
                Case(Screen::Home, [stateSignal, setActiveFn, onActionFn] {
                    return HomeScreen{.state = stateSignal, .onNavigate = setActiveFn, .onAction = onActionFn};
                }),
                Case(Screen::Map, [stateSignal] {
                    return MapScreen{.state = stateSignal};
                }),
                Case(Screen::Music, [stateSignal, onActionFn] {
                    return MusicScreen{.state = stateSignal, .onAction = onActionFn};
                }),
                Case(Screen::Phone, [stateSignal] {
                    return PhoneScreen{.state = stateSignal};
                }),
                Case(Screen::Climate, [stateSignal, setClimateFn] {
                    return ClimateScreen{.state = stateSignal, .onChange = setClimateFn};
                }),
                Case(Screen::Vehicle, [stateSignal, setVehicleControlsFn] {
                    return VehicleScreen{.state = stateSignal, .onChange = setVehicleControlsFn};
                }),
                Case(Screen::Settings, [] {
                    return SettingsScreen{};
                }),
            },
            [stateSignal, setActiveFn, onActionFn] {
                return HomeScreen{.state = stateSignal, .onNavigate = setActiveFn, .onAction = onActionFn};
            }
        );
    }
};

} // namespace car
