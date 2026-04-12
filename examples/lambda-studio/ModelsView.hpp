#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <functional>

#include "AppState.hpp"

using namespace flux;

namespace lambda {

struct ModelRow : ViewModifiers<ModelRow> {
    LocalModel model;
    bool active = false;
    bool loading = false;
    std::function<void()> onLoad;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        bool const hovered = useHover();

        return HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Center,
            .children = children(
                VStack {
                    .spacing = theme.space1,
                    .alignment = Alignment::Start,
                    .children = children(
                        Text {
                            .text = model.name,
                            .font = theme.fontLabel,
                            .color = active ? theme.colorAccent : theme.colorTextPrimary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                        },
                        Text {
                            .text = formatModelSize(model.sizeBytes) + (model.path.empty() ? "" : "  •  " + model.path),
                            .font = theme.fontBodySmall,
                            .color = theme.colorTextSecondary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        }
                    )
                }
                    .flex(1.f, 1.f),
                Button {
                    .label = active ? "Loaded" : loading ? "Loading..." :
                                                           "Load",
                    .variant = active ? ButtonVariant::Ghost : ButtonVariant::Secondary,
                    .disabled = active || loading,
                    .onTap = onLoad,
                }
            )
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(hovered ? theme.colorSurfaceHover : theme.colorSurfaceOverlay))
            .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
            .cornerRadius(theme.radiusLarge);
    }
};

struct ModelsView : ViewModifiers<ModelsView> {
    AppState state;
    std::function<void()> onRefresh;
    std::function<void(std::string const &, std::string const &)> onLoad;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        std::vector<Element> rows;
        rows.reserve(state.localModels.size());

        for (LocalModel const &model : state.localModels) {
            bool const isActive = !state.loadedModelPath.empty() && model.path == state.loadedModelPath;
            bool const isLoading = state.modelLoading && !state.pendingModelPath.empty() && model.path == state.pendingModelPath;
            rows.push_back(ModelRow {
                .model = model,
                .active = isActive,
                .loading = isLoading,
                .onLoad = [onLoad = onLoad, path = model.path, name = model.name] {
                    if (onLoad) {
                        onLoad(path, name);
                    }
                },
            });
        }

        if (rows.empty()) {
            rows.push_back(
                VStack {
                    .spacing = theme.space2,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = "No local models found",
                            .font = theme.fontHeading,
                            .color = theme.colorTextPrimary,
                            .horizontalAlignment = HorizontalAlignment::Center,
                        },
                        Text {
                            .text = "Drop `.gguf` files into your configured model directories, then refresh.",
                            .font = theme.fontBodySmall,
                            .color = theme.colorTextSecondary,
                            .horizontalAlignment = HorizontalAlignment::Center,
                            .wrapping = TextWrapping::Wrap,
                        }
                    )
                }
                    .padding(theme.space6)
                    .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                    .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                    .cornerRadius(theme.radiusLarge)
            );
        }

        return VStack {
            .spacing = theme.space4,
            .alignment = Alignment::Stretch,
            .children = children(
                HStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Center,
                    .children = children(
                        VStack {
                            .spacing = theme.space1,
                            .alignment = Alignment::Start,
                            .children = children(
                                Text {
                                    .text = "Models",
                                    .font = theme.fontHeading,
                                    .color = theme.colorTextPrimary,
                                },
                                Text {
                                    .text = state.modelLoading && !state.pendingModelName.empty() ?
                                                "Loading " + state.pendingModelName :
                                            state.loadedModelName.empty() ? "No model loaded" :
                                                                            "Loaded: " + state.loadedModelName,
                                    .font = theme.fontBodySmall,
                                    .color = theme.colorTextSecondary,
                                }
                            )
                        }
                            .flex(1.f, 1.f),
                        Button {
                            .label = state.refreshingModels ? "Refreshing..." : "Refresh",
                            .variant = ButtonVariant::Secondary,
                            .disabled = state.refreshingModels,
                            .onTap = onRefresh,
                        }
                    )
                },
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = theme.space3,
                            .children = std::move(rows),
                        }
                    )
                }
                    .flex(1.f, 1.f)
            )
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(theme.colorBackground));
    }
};

} // namespace lambda
