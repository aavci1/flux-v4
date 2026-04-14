#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "AppState.hpp"
#include "SharedViews.hpp"

using namespace flux;

namespace lambda {

struct ModelRow : ViewModifiers<ModelRow> {
    LocalModel model;
    bool active = false;
    bool loading = false;
    bool deleting = false;
    std::function<void()> onLoad;
    std::function<void()> onDelete;

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
                        .disabled = active || loading || deleting,
                        .onTap = onLoad,
                    },
                    IconButton {
                        .icon = IconName::Delete,
                        .disabled = deleting,
                        .style = {
                            .size = theme.fontHeading.size,
                            .weight = theme.fontLabel.weight,
                            .color = deleting ? theme.colorTextDisabled : theme.colorDanger,
                        },
                        .onTap = onDelete,
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
    bool cancelEnabled = false;
    std::function<void()> onRetry;
    std::function<void()> onCancel;
    std::function<void()> onRemove;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        std::string title = job.filePath;
        if (!job.repoId.empty()) {
            title += "  •  " + job.repoId;
        }

        std::string meta = downloadJobStatusLabel(job.status);
        if (job.status == DownloadJobStatus::Running) {
            meta += "  •  " + shared_ui::formatTransferProgress(job.downloadedBytes, job.totalBytes);
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
            infoChildren.push_back(
                ProgressBar {
                    .progress = downloadJobProgress(job),
                    .style = {
                        .activeColor = theme.colorAccent,
                        .inactiveColor = theme.colorSurfaceHover,
                        .trackHeight = 6.f,
                    },
                }.size(112.f, 0.f)
            );
        }

        return ListRow {
            .content = HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Center,
                .children = children(
                    VStack {
                        .spacing = theme.space2,
                        .alignment = Alignment::Start,
                        .children = std::move(infoChildren)
                    }
                        .flex(1.f, 1.f),
                    job.status == DownloadJobStatus::Running
                        ? Element {LinkButton {
                              .label = "Stop",
                              .disabled = !cancelEnabled,
                              .onTap = onCancel,
                          }}
                        : job.status == DownloadJobStatus::Failed
                              ? Element {HStack {
                                    .spacing = theme.space2,
                                    .alignment = Alignment::Center,
                                    .children = children(
                                        LinkButton {
                                            .label = "Retry",
                                            .disabled = !retryEnabled,
                                            .onTap = onRetry,
                                        },
                                        IconButton {
                                            .icon = IconName::Delete,
                                            .style = {
                                                .size = theme.fontBody.size,
                                                .weight = theme.fontLabel.weight,
                                                .color = theme.colorTextSecondary,
                                            },
                                            .onTap = onRemove,
                                        }
                                    )
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
    std::function<void(std::string const &, std::string const &, std::string const &)> onDeleteModel;
    std::function<void(std::string, std::string)> onRetryDownload;
    std::function<void(std::string const &)> onCancelDownload;
    std::function<void(std::string const &)> onRemoveDownload;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        std::vector<Element> localRows;
        localRows.reserve(state.localModels.size());
        for (LocalModel const &model : state.localModels) {
            bool const isActive = !state.loadedModelPath.empty() && model.path == state.loadedModelPath;
            bool const isLoading = state.modelLoading && !state.pendingModelPath.empty() &&
                                   model.path == state.pendingModelPath;
            bool const isDeleting = state.modelDeleting && !state.pendingDeleteModelPath.empty() &&
                                    model.path == state.pendingDeleteModelPath;
            localRows.push_back(ModelRow {
                .model = model,
                .active = isActive,
                .loading = isLoading,
                .deleting = isDeleting,
                .onLoad = [onLoad = onLoad, path = model.path, name = model.name] {
                    if (onLoad) {
                        onLoad(path, name);
                    }
                },
                .onDelete = [onDeleteModel = onDeleteModel, path = model.path, repo = model.repo, name = model.name] {
                    if (onDeleteModel) {
                        onDeleteModel(path, repo, name);
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
                .cancelEnabled = state.downloadingModel && job.id == state.pendingDownloadJobId,
                .onRetry = [onRetryDownload = onRetryDownload, repoId = job.repoId, filePath = job.filePath] {
                    if (onRetryDownload) {
                        onRetryDownload(repoId, filePath);
                    }
                },
                .onCancel = [onCancelDownload = onCancelDownload, jobId = job.id] {
                    if (onCancelDownload) {
                        onCancelDownload(jobId);
                    }
                },
                .onRemove = [onRemoveDownload = onRemoveDownload, jobId = job.id] {
                    if (onRemoveDownload) {
                        onRemoveDownload(jobId);
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
                              ? Element {EmptyStatePanel {
                                    .title = "No Models",
                                    .detail = "Download models from Hugging Face using the Hub module.",
                                }.flex(1.f, 1.f, 0.f)}
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
                Divider {
                    .orientation = Divider::Orientation::Horizontal
                },
                std::move(content)
            )
        }
            .fill(FillStyle::solid(theme.colorSurfaceOverlay));
    }
};

} // namespace lambda
