#pragma once

#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

using namespace flux;

struct Divider: ViewModifiers<Divider> {
    auto body() const {
        Theme const& theme = useEnvironment<Theme>();
        return Rectangle {
        }
        .size(0.f, 1.f)
        .cornerRadius(1.f)
        .fill(FillStyle::solid(theme.colorBorder));
    }
};