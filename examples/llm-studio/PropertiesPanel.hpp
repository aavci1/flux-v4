#pragma once

#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "Divider.hpp"

using namespace flux;

struct PropertiesPanel : ViewModifiers<PropertiesPanel> {
    std::string host {""};
    std::string model {""};

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        return VStack {
            .spacing = 8.f,
            .children = children(
                Text {
                    .text = "Properties",
                    .style = theme.typeTitle,
                    .color = theme.colorTextPrimary
                }.padding(16.f, 8.f, 8.f, 8.f),
                Divider {},
                Text {
                    .text = std::string("Model: ") + host,
                    .style = theme.typeBody,
                    .color = theme.colorTextSecondary,
                    .verticalAlignment = VerticalAlignment::Top,
                    .wrapping = TextWrapping::Wrap,
                }.padding(8.f, 16.f, 4.f, 16.f),
                Text {
                    .text = std::string("Model: ") + model,
                    .style = theme.typeBody,
                    .color = theme.colorTextSecondary,
                    .verticalAlignment = VerticalAlignment::Top,
                    .wrapping = TextWrapping::Wrap,
                }.padding(4.f, 16.f, 8.f, 16.f)
            )
        }.size(320.f, 0.f);
    }
};