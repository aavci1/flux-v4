#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include "AppState.hpp"

using namespace flux;

namespace lambda {

struct SettingsView : ViewModifiers<SettingsView> {
    AppState state;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        auto row = [&](std::string label, std::string value) {
            return HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Center,
                .children = children(
                    Text {
                        .text = std::move(label),
                        .font = theme.fontLabel,
                        .color = theme.colorTextPrimary,
                        .horizontalAlignment = HorizontalAlignment::Leading,
                    }
                        .size(160.f, 0.f),
                    Text {
                        .text = std::move(value),
                        .font = theme.fontBodySmall,
                        .color = theme.colorTextSecondary,
                        .horizontalAlignment = HorizontalAlignment::Leading,
                        .wrapping = TextWrapping::Wrap,
                    }
                        .flex(1.f, 1.f)
                )
            };
        };

        return VStack {
            .spacing = theme.space4,
            .alignment = Alignment::Stretch,
            .children = children(
                Text {
                    .text = "Settings",
                    .font = theme.fontHeading,
                    .color = theme.colorTextPrimary,
                },
                VStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        row("Loaded model", state.loadedModelName.empty() ? "None" : state.loadedModelName),
                        row("Model path", state.loadedModelPath.empty() ? "No active model" : state.loadedModelPath),
                        row("Chats", std::to_string(state.chats.size())),
                        row("Local models", std::to_string(state.localModels.size())),
                        row("Status", state.statusText.empty() ? "Idle" : state.statusText),
                        row("Last error", state.errorText.empty() ? "No errors" : state.errorText)
                    )
                }
                    .padding(theme.space4)
                    .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                    .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                    .cornerRadius(theme.radiusLarge),
                Spacer {}
            )
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(theme.colorBackground));
    }
};

} // namespace lambda
