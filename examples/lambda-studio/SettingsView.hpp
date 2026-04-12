#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

using namespace flux;

namespace lambda {

struct SettingsView : ViewModifiers<SettingsView> {
    const std::string name = "Settings";
    const IconName icon = IconName::Settings;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        return Text {
            .text = name,
            .font = theme.fontDisplay,
            .color = theme.colorTextPrimary,
            .horizontalAlignment = HorizontalAlignment::Center,
            .verticalAlignment = VerticalAlignment::Center,
        };
    }
};

} // namespace lambda
