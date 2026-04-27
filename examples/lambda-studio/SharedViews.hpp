#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <string>

#include "AppState.hpp"

using namespace flux;

namespace lambda {

namespace shared_ui {

inline int progressPercent(std::size_t downloadedBytes, std::size_t totalBytes) {
    if (totalBytes == 0) {
        return 0;
    }
    return static_cast<int>(
        std::clamp(
            (100.0 * static_cast<double>(downloadedBytes)) / static_cast<double>(totalBytes),
            0.0,
            100.0));
}

inline std::string formatTransferProgress(std::size_t downloadedBytes, std::size_t totalBytes) {
    if (totalBytes == 0) {
        return "Starting download...";
    }
    return formatModelSize(downloadedBytes) + " / " + formatModelSize(totalBytes) +
           "  •  " + std::to_string(progressPercent(downloadedBytes, totalBytes)) + "%";
}

} // namespace shared_ui

struct EmptyStatePanel : ViewModifiers<EmptyStatePanel> {
    std::string title;
    std::string detail;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();

        return VStack {
            .spacing = theme().space2,
            .alignment = Alignment::Center,
            .children = children(
                Text {
                    .text = title,
                    .font = Font::title(),
                    .color = Color::secondary(),
                    .horizontalAlignment = HorizontalAlignment::Center,
                },
                Text {
                    .text = detail,
                    .font = Font::body(),
                    .color = Color::secondary(),
                    .horizontalAlignment = HorizontalAlignment::Center,
                    .wrapping = TextWrapping::Wrap,
                }
            )
        }
            .padding(theme().space6)
            .flex(1.f, 1.f);
    }
};

struct LabeledValueRow : ViewModifiers<LabeledValueRow> {
    std::string label;
    std::string value;
    float labelWidth = 96.f;
    float spacing = 0.f;
    bool emphasize = false;
    int maxLines = 2;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();

        return HStack {
            .spacing = spacing > 0.f ? spacing : theme().space2,
            .alignment = Alignment::Start,
            .children = children(
                Text {
                    .text = label,
                    .font = Font::caption(),
                    .color = Color::tertiary(),
                    .horizontalAlignment = HorizontalAlignment::Leading,
                }
                    .size(labelWidth, 0.f),
                Text {
                    .text = value,
                    .font = emphasize ? Font::headline() : Font::footnote(),
                    .color = emphasize ? Color::primary() : Color::secondary(),
                    .horizontalAlignment = HorizontalAlignment::Leading,
                    .wrapping = TextWrapping::Wrap,
                    .maxLines = maxLines,
                }
                    .flex(1.f, 1.f)
            )
        };
    }
};

} // namespace lambda
