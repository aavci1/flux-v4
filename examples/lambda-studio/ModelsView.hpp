#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

using namespace flux;

namespace lambda {

struct ModelsView : ViewModifiers<ModelsView> {
    const std::string name = "Models";
    const IconName icon = IconName::ModelTraining;

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
