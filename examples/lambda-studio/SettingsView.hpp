#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "AppState.hpp"
#include "SharedViews.hpp"

using namespace flux;

namespace lambda {

namespace settings_view_detail {

enum class StatusTone {
    Accent,
    Success,
    Warning,
    Danger,
};

inline int runningDownloadCount(AppState const &state) {
    return static_cast<int>(std::count_if(
        state.recentDownloadJobs.begin(),
        state.recentDownloadJobs.end(),
        [](DownloadJob const &job) {
            return job.status == DownloadJobStatus::Running;
        }
    ));
}

inline int failedDownloadCount(AppState const &state) {
    return static_cast<int>(std::count_if(
        state.recentDownloadJobs.begin(),
        state.recentDownloadJobs.end(),
        [](DownloadJob const &job) {
            return job.status == DownloadJobStatus::Failed;
        }
    ));
}

inline int activeTaskCount(AppState const &state) {
    int count = 0;
    count += state.refreshingModels ? 1 : 0;
    count += state.searchingRemoteModels ? 1 : 0;
    count += state.loadingRemoteModelFiles ? 1 : 0;
    count += state.loadingRemoteRepoDetail ? 1 : 0;
    count += state.downloadingModel ? 1 : 0;
    count += state.modelLoading ? 1 : 0;
    count += runningDownloadCount(state);
    return count;
}

inline std::string primaryStatusText(AppState const &state) {
    if (!state.errorText.empty()) {
        return "Attention needed";
    }
    if (state.modelLoading) {
        return "Loading model";
    }
    if (state.downloadingModel) {
        return "Downloading model";
    }
    if (state.searchingRemoteModels) {
        return "Searching hub";
    }
    if (state.refreshingModels) {
        return "Refreshing models";
    }
    if (state.loadingRemoteRepoDetail || state.loadingRemoteModelFiles) {
        return "Fetching details";
    }
    if (state.notice.has_value()) {
        return "Notice available";
    }
    if (!state.loadedModelName.empty()) {
        return "Ready";
    }
    return "Idle";
}

inline StatusTone primaryStatusTone(AppState const &state) {
    if (!state.errorText.empty()) {
        return StatusTone::Danger;
    }
    if (state.notice.has_value()) {
        return StatusTone::Warning;
    }
    if (state.modelLoading || state.downloadingModel || state.searchingRemoteModels || state.refreshingModels ||
        state.loadingRemoteRepoDetail || state.loadingRemoteModelFiles) {
        return StatusTone::Accent;
    }
    return !state.loadedModelName.empty() ? StatusTone::Success : StatusTone::Warning;
}

inline Color toneForeground(Theme const &theme, StatusTone tone) {
    switch (tone) {
    case StatusTone::Success:
        return theme.colorSuccess;
    case StatusTone::Warning:
        return theme.colorWarning;
    case StatusTone::Danger:
        return theme.colorDanger;
    case StatusTone::Accent:
    default:
        return theme.colorAccent;
    }
}

inline Color toneBackground(Theme const &theme, StatusTone tone) {
    switch (tone) {
    case StatusTone::Success:
        return theme.colorSuccessSubtle;
    case StatusTone::Warning:
        return theme.colorWarningSubtle;
    case StatusTone::Danger:
        return theme.colorDangerSubtle;
    case StatusTone::Accent:
    default:
        return theme.colorAccentSubtle;
    }
}

inline std::string loadedModelSummary(AppState const &state) {
    if (!state.loadedModelName.empty()) {
        return state.loadedModelName;
    }
    if (!state.pendingModelName.empty()) {
        return state.pendingModelName;
    }
    return "No active model";
}

inline std::string loadedModelDetail(AppState const &state) {
    if (!state.loadedModelPath.empty()) {
        return state.loadedModelPath;
    }
    if (!state.pendingModelPath.empty()) {
        return state.pendingModelPath;
    }
    return "Choose a local model in Chats or Models to make it active.";
}

inline std::string pendingModelSummary(AppState const &state) {
    if (state.pendingModelName.empty() && state.pendingModelPath.empty()) {
        return "None";
    }
    if (!state.pendingModelName.empty()) {
        return state.pendingModelName;
    }
    return state.pendingModelPath;
}

inline std::string formatCountLabel(int count, std::string singular, std::string plural) {
    return std::to_string(count) + " " + (count == 1 ? singular : plural);
}

inline Element statCard(Theme const &theme, std::string label, std::string value, std::string detail,
                        Color accent) {
    return VStack {
        .spacing = theme.space2,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(label),
                .font = theme.fontLabelSmall,
                .color = theme.colorTextMuted,
            },
            Text {
                .text = std::move(value),
                .font = theme.fontHeading,
                .color = theme.colorTextPrimary,
                .horizontalAlignment = HorizontalAlignment::Leading,
            },
            Text {
                .text = std::move(detail),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .horizontalAlignment = HorizontalAlignment::Leading,
                .wrapping = TextWrapping::Wrap,
                .maxLines = 2,
            }
        )
    }
        .padding(theme.space4)
        .fill(FillStyle::solid(theme.colorSurfaceOverlay))
        .stroke(StrokeStyle::solid(accent, 1.f))
        .cornerRadius(theme.radiusXLarge)
        .flex(1.f, 1.f, 0.f);
}

inline Element sectionCard(Theme const &theme, std::string eyebrow, std::string title, std::string detail,
                           std::vector<Element> rows) {
    std::vector<Element> children;
    children.reserve(rows.size() + 3);
    children.push_back(
        Text {
            .text = std::move(eyebrow),
            .font = theme.fontLabelSmall,
            .color = theme.colorTextMuted,
            .horizontalAlignment = HorizontalAlignment::Leading,
        }
    );
    children.push_back(
        Text {
            .text = std::move(title),
            .font = theme.fontTitle,
            .color = theme.colorTextPrimary,
            .horizontalAlignment = HorizontalAlignment::Leading,
        }
    );
    children.push_back(
        Text {
            .text = std::move(detail),
            .font = theme.fontBodySmall,
            .color = theme.colorTextSecondary,
            .horizontalAlignment = HorizontalAlignment::Leading,
            .wrapping = TextWrapping::Wrap,
        }
    );

    for (std::size_t i = 0; i < rows.size(); ++i) {
        children.push_back(
            Divider {
                .orientation = Divider::Orientation::Horizontal,
            }
        );
        children.push_back(std::move(rows[i]));
    }

    return VStack {
        .spacing = theme.space3,
        .alignment = Alignment::Stretch,
        .children = std::move(children),
    }
        .padding(theme.space4)
        .fill(FillStyle::solid(theme.colorSurfaceOverlay))
        .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
        .cornerRadius(theme.radiusXLarge);
}

} // namespace settings_view_detail

struct SettingsView : ViewModifiers<SettingsView> {
    AppState state;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        using namespace settings_view_detail;

        std::string const statusText = primaryStatusText(state);
        StatusTone const statusTone = primaryStatusTone(state);
        int const downloadsRunning = runningDownloadCount(state);
        int const downloadsFailed = failedDownloadCount(state);
        int const activeTasks = activeTaskCount(state);

        std::vector<Element> overviewStats;
        overviewStats.reserve(3);
        overviewStats.push_back(statCard(
            theme,
            "Conversations",
            std::to_string(state.chats.size()),
            state.chats.empty() ? "No chats" : formatCountLabel(static_cast<int>(state.chats.size()), "thread", "threads"),
            theme.colorAccentSubtle
        ));
        overviewStats.push_back(statCard(
            theme,
            "Local Library",
            std::to_string(state.localModels.size()),
            state.localModels.empty() ? "No local models discovered" :
                                        formatCountLabel(static_cast<int>(state.localModels.size()), "model", "models"),
            theme.colorSuccessSubtle
        ));
        overviewStats.push_back(statCard(
            theme,
            "Background Work",
            std::to_string(activeTasks),
            activeTasks == 0 ? "Everything is quiet right now" :
                               formatCountLabel(activeTasks, "active task", "active tasks"),
            downloadsFailed > 0 ? theme.colorDangerSubtle : theme.colorWarningSubtle
        ));

        std::vector<Element> runtimeRows;
        runtimeRows.reserve(4);
        runtimeRows.push_back(LabeledValueRow {
            .label = "Active model",
            .value = loadedModelSummary(state),
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .emphasize = true,
        });
        runtimeRows.push_back(LabeledValueRow {
            .label = "Pending model",
            .value = pendingModelSummary(state),
            .labelWidth = 126.f,
            .spacing = theme.space3,
        });
        runtimeRows.push_back(LabeledValueRow {
            .label = "Current module",
            .value = moduleTitle(state.currentModule),
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .maxLines = 1,
        });
        runtimeRows.push_back(LabeledValueRow {
            .label = "Status message",
            .value = state.statusText.empty() ? "No background status message." : state.statusText,
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .maxLines = 3,
        });

        std::vector<Element> libraryRows;
        libraryRows.reserve(4);
        libraryRows.push_back(LabeledValueRow {
            .label = "Model path",
            .value = loadedModelDetail(state),
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .maxLines = 4,
        });
        libraryRows.push_back(LabeledValueRow {
            .label = "Downloads running",
            .value = downloadsRunning == 0 ? "None" : formatCountLabel(downloadsRunning, "download", "downloads"),
            .labelWidth = 126.f,
            .spacing = theme.space3,
        });
        libraryRows.push_back(LabeledValueRow {
            .label = "Failed downloads",
            .value = downloadsFailed == 0 ? "No failed downloads" : formatCountLabel(downloadsFailed, "failed job", "failed jobs"),
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .emphasize = downloadsFailed > 0,
        });
        libraryRows.push_back(LabeledValueRow {
            .label = "Recent jobs tracked",
            .value = std::to_string(state.recentDownloadJobs.size()),
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .maxLines = 1,
        });

        std::vector<Element> diagnosticsRows;
        diagnosticsRows.reserve(4);
        diagnosticsRows.push_back(LabeledValueRow {
            .label = "Last error",
            .value = state.errorText.empty() ? "No recent errors." : state.errorText,
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .emphasize = !state.errorText.empty(),
            .maxLines = 4,
        });
        diagnosticsRows.push_back(LabeledValueRow {
            .label = "Notice",
            .value = state.notice.has_value() ? (state.notice->title + "  •  " + state.notice->detail) : "No pending notices.",
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .emphasize = state.notice.has_value(),
            .maxLines = 4,
        });
        diagnosticsRows.push_back(LabeledValueRow {
            .label = "Hub search",
            .value = state.modelSearchQuery.empty() && state.modelSearchAuthor.empty()
                ? "No active search filters"
                : "Query: " + (state.modelSearchQuery.empty() ? "Any" : state.modelSearchQuery) +
                      "  •  Author: " + (state.modelSearchAuthor.empty() ? "Any" : state.modelSearchAuthor),
            .labelWidth = 126.f,
            .spacing = theme.space3,
            .maxLines = 3,
        });
        diagnosticsRows.push_back(LabeledValueRow {
            .label = "Selected remote repo",
            .value = state.selectedRemoteRepoId.empty() ? "Nothing selected" : state.selectedRemoteRepoId,
            .labelWidth = 126.f,
            .spacing = theme.space3,
        });

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = theme.space5,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        VStack {
                            .spacing = theme.space2,
                            .alignment = Alignment::Start,
                            .children = children(
                                Text {
                                    .text = "Studio settings",
                                    .font = theme.fontLabelSmall,
                                    .color = theme.colorTextMuted,
                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                },
                                Text {
                                    .text = "Settings",
                                    .font = theme.fontDisplay,
                                    .color = theme.colorTextPrimary,
                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                },
                                Text {
                                    .text = "A cleaner snapshot of the local workspace, runtime state, and background activity.",
                                    .font = theme.fontBody,
                                    .color = theme.colorTextSecondary,
                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                    .wrapping = TextWrapping::Wrap,
                                }
                            )
                        },
                        VStack {
                            .spacing = theme.space4,
                            .alignment = Alignment::Stretch,
                            .children = children(
                                HStack {
                                    .spacing = theme.space3,
                                    .alignment = Alignment::Center,
                                    .children = children(
                                        VStack {
                                            .spacing = theme.space2,
                                            .alignment = Alignment::Start,
                                            .children = children(
                                                Text {
                                                    .text = "Runtime overview",
                                                    .font = theme.fontLabelSmall,
                                                    .color = theme.colorTextMuted,
                                                },
                                                Text {
                                                    .text = loadedModelSummary(state),
                                                    .font = theme.fontHeading,
                                                    .color = theme.colorTextPrimary,
                                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                                    .wrapping = TextWrapping::Wrap,
                                                },
                                                Text {
                                                    .text = loadedModelDetail(state),
                                                    .font = theme.fontBodySmall,
                                                    .color = theme.colorTextSecondary,
                                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                                    .wrapping = TextWrapping::Wrap,
                                                    .maxLines = 4,
                                                }
                                            )
                                        }
                                            .flex(1.f, 1.f, 0.f),
                                        Badge {
                                            .label = statusText,
                                            .style = {
                                                .font = theme.fontLabelSmall,
                                                .foregroundColor = toneForeground(theme, statusTone),
                                                .backgroundColor = toneBackground(theme, statusTone),
                                            },
                                        }
                                    )
                                },
                                HStack {
                                    .spacing = theme.space3,
                                    .alignment = Alignment::Stretch,
                                    .children = std::move(overviewStats)
                                }
                            )
                        }
                            .padding(theme.space4)
                            .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                            .stroke(StrokeStyle::solid(toneBackground(theme, statusTone), 1.f))
                            .cornerRadius(theme.radiusXLarge),
                        sectionCard(
                            theme,
                            "Runtime",
                            "Session state",
                            "The current app session, active module, and model lifecycle at a glance.",
                            std::move(runtimeRows)
                        ),
                        sectionCard(
                            theme,
                            "Library",
                            "Models and jobs",
                            "Track the local model library and any background transfers in one place.",
                            std::move(libraryRows)
                        ),
                        sectionCard(
                            theme,
                            "Diagnostics",
                            "Operational context",
                            "Useful context for debugging the current session without digging through raw app state.",
                            std::move(diagnosticsRows)
                        )
                    )
                }
                    .padding(theme.space5)
            )
        }
            .fill(FillStyle::solid(theme.colorBackground));
    }
};

} // namespace lambda
