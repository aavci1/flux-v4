#pragma once

#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "Divider.hpp"

using namespace flux;

struct MenuPanel : ViewModifiers<MenuPanel> {
    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        return VStack {
            .spacing = 8.f,
            .alignment = Alignment::Stretch,
            .children = children(
                Divider {}.padding(54.f, 8.f, 0.f, 8.f),
                Menu {}
            )
        }.width(240.f);
    }
};