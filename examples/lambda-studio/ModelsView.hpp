#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "AppState.hpp"

using namespace flux;

namespace lambda {

namespace models_view_detail {

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

inline Element progressBar(
    Theme const &theme,
    float progress
) {
    float const clamped = std::clamp(progress, 0.f, 1.f);
    constexpr float trackWidth = 112.f;
    constexpr float trackHeight = 6.f;
    return Element {ZStack {
        .horizontalAlignment = Alignment::Start,
        .verticalAlignment = Alignment::Start,
        .children = children(
            Rectangle {}
                .fill(FillStyle::solid(theme.colorSurfaceHover))
                .size(trackWidth, trackHeight)
                .cornerRadius(CornerRadius {trackHeight * 0.5f}),
            Rectangle {}
                .fill(FillStyle::solid(theme.colorAccent))
                .size(trackWidth * clamped, trackHeight)
                .cornerRadius(CornerRadius {trackHeight * 0.5f})
        )
    }}
        .size(trackWidth, trackHeight);
}

inline Element placeholderPanel(
    Theme const &theme,
    std::string title,
    std::string detail
) {
    return VStack {
        .spacing = theme.space2,
        .alignment = Alignment::Center,
        .children = children(
            Text {
                .text = std::move(title),
                .font = theme.fontHeading,
                .color = theme.colorTextPrimary,
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            Text {
                .text = std::move(detail),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .horizontalAlignment = HorizontalAlignment::Center,
                .wrapping = TextWrapping::Wrap,
            }
        )
    }
        .padding(theme.space6)
        .flex(1.f, 1.f);
}

} // namespace models_view_detail

struct ModelRow : ViewModifiers<ModelRow> {
    LocalModel model;
    bool active = false;
    bool loading = false;
    std::function<void()> onLoad;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        std::string detail = formatModelSize(model.sizeBytes);
        if (!model.repo.empty()) {
            detail += "  •  " + model.repo;
        }
        if (!model.path.empty()) {
            detail += "  •  " + model.path;
        }

        return ListRow {
            .content = HStack {
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
                                .text = std::move(detail),
                                .font = theme.fontBodySmall,
                                .color = theme.colorTextSecondary,
                                .horizontalAlignment = HorizontalAlignment::Leading,
                                .wrapping = TextWrapping::Wrap,
                                .maxLines = 2,
                            }
                        )
                    }
                        .flex(1.f, 1.f),
                    LinkButton {
                        .label = active ? "Loaded" : loading ? "Loading..." : "Load",
                        .disabled = active || loading,
                        .onTap = onLoad,
                    }
                )
            },
            .selected = active,
        };
    }
};

struct DownloadJobRow : ViewModifiers<DownloadJobRow> {
    DownloadJob job;
    bool retryEnabled = false;
    std::function<void()> onRetry;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        std::string title = job.filePath;
        if (!job.repoId.empty()) {
            title += "  •  " + job.repoId;
        }

        std::string meta = downloadJobStatusLabel(job.status);
        if (job.status == DownloadJobStatus::Running) {
            meta += "  •  " + models_view_detail::formatTransferProgress(job.downloadedBytes, job.totalBytes);
        } else if (!job.localPath.empty()) {
            meta += "  •  " + job.localPath;
        } else if (!job.error.empty()) {
            meta += "  •  " + job.error;
        }

        std::vector<Element> infoChildren;
        infoChildren.reserve(job.status == DownloadJobStatus::Running && job.totalBytes > 0 ? 3 : 2);
        infoChildren.push_back(
            Text {
                .text = title,
                .font = theme.fontLabelSmall,
                .color = job.status == DownloadJobStatus::Failed ? theme.colorDanger : theme.colorTextPrimary,
                .wrapping = TextWrapping::Wrap,
                .maxLines = 2,
            }
        );
        infoChildren.push_back(
            Text {
                .text = meta,
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .wrapping = TextWrapping::Wrap,
                .maxLines = 2,
            }
        );
        if (job.status == DownloadJobStatus::Running && job.totalBytes > 0) {
            infoChildren.push_back(models_view_detail::progressBar(theme, downloadJobProgress(job)));
        }

        return ListRow {
            .content = HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Center,
                .children = children(
                    VStack {
                        .spacing = theme.space1,
                        .alignment = Alignment::Start,
                        .children = std::move(infoChildren)
                    }
                        .flex(1.f, 1.f),
                    job.status == DownloadJobStatus::Failed
                        ? Element {LinkButton {
                              .label = "Retry",
                              .disabled = !retryEnabled,
                              .onTap = onRetry,
                          }}
                        : Element {Spacer {}.size(0.f, 0.f)}
                )
            },
        };
    }
};

struct ModelsView : ViewModifiers<ModelsView> {
    AppState state;
    std::function<void()> onRefresh;
    std::function<void(std::string const &, std::string const &)> onLoad;
    std::function<void(std::string, std::string)> onRetryDownload;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        std::vector<Element> localRows;
        localRows.reserve(state.localModels.size());
        for (LocalModel const &model : state.localModels) {
            bool const isActive = !state.loadedModelPath.empty() && model.path == state.loadedModelPath;
            bool const isLoading = state.modelLoading && !state.pendingModelPath.empty() &&
                                   model.path == state.pendingModelPath;
            localRows.push_back(ModelRow {
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

        std::vector<Element> downloadRows;
        downloadRows.reserve(state.recentDownloadJobs.size());
        for (DownloadJob const &job : state.recentDownloadJobs) {
            if (job.status == DownloadJobStatus::Completed) {
                continue;
            }
            downloadRows.push_back(DownloadJobRow {
                .job = job,
                .retryEnabled = !state.downloadingModel && !job.repoId.empty() && !job.filePath.empty(),
                .onRetry = [onRetryDownload = onRetryDownload, repoId = job.repoId, filePath = job.filePath] {
                    if (onRetryDownload) {
                        onRetryDownload(repoId, filePath);
                    }
                },
            });
        }

        std::vector<Element> localLibraryRows;
        localLibraryRows.reserve(downloadRows.size() + localRows.size());
        for (Element &row : downloadRows) {
            localLibraryRows.push_back(std::move(row));
        }
        for (Element &row : localRows) {
            localLibraryRows.push_back(std::move(row));
        }

        Element content = localLibraryRows.empty()
                              ? models_view_detail::placeholderPanel(
                                    theme,
                                    "No local models found",
                                    "Lambda Studio indexes the Hugging Face cache first. Downloads and active transfers appear here."
                                )
                              : Element {ListView {.rows = std::move(localLibraryRows)}.flex(1.f, 1.f, 0.f)};

        return VStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                HStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = "Models",
                            .font = theme.fontHeading,
                            .color = theme.colorTextPrimary,
                        }
                            .flex(1.f, 1.f),
                        LinkButton {
                            .label = state.refreshingModels ? "Refreshing..." : "Refresh",
                            .disabled = state.refreshingModels,
                            .onTap = onRefresh,
                        }
                    )
                }
                    .padding(theme.space4),
                Rectangle {}
                    .size(0.f, 1.f)
                    .fill(FillStyle::solid(theme.colorBorderSubtle)),
                std::move(content)
            )
        }
            .fill(FillStyle::solid(theme.colorSurfaceOverlay));
    }
};

} // namespace lambda
