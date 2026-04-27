#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "AppState.hpp"
#include "SharedViews.hpp"

using namespace flux;

namespace lambda {

namespace hub_view_detail {

inline std::string formatCompactCount(std::int64_t value) {
    if (value >= 1'000'000'000) {
        return std::to_string(value / 1'000'000'000) + "B";
    }
    if (value >= 1'000'000) {
        return std::to_string(value / 1'000'000) + "M";
    }
    if (value >= 1'000) {
        return std::to_string(value / 1'000) + "K";
    }
    return std::to_string(value);
}

} // namespace hub_view_detail

struct RemoteModelRow : ViewModifiers<RemoteModelRow> {
    RemoteModel model;
    bool selected = false;
    std::function<void()> onTap;

    auto body() const {
        auto theme = useEnvironmentReactive<ThemeKey>();

        std::string meta = model.author.empty() ? model.id : model.author;
        if (!model.pipelineTag.empty()) {
            meta += "  •  " + model.pipelineTag;
        }
        meta += "  •  " + hub_view_detail::formatCompactCount(model.downloads) + " downloads";
        meta += "  •  " + hub_view_detail::formatCompactCount(model.likes) + " likes";

        std::string tags = joinedTags(model.tags);
        std::vector<Element> contentChildren;
        contentChildren.reserve(tags.empty() ? 2 : 3);
        contentChildren.push_back(
            Text {
                .text = model.id,
                .font = Font::headline(),
                .color = selected ? Color::accent() : Color::primary(),
                .horizontalAlignment = HorizontalAlignment::Leading,
                .wrapping = TextWrapping::Wrap,
            }
        );
        contentChildren.push_back(
            Text {
                .text = std::move(meta),
                .font = Font::footnote(),
                .color = Color::secondary(),
                .horizontalAlignment = HorizontalAlignment::Leading,
                .wrapping = TextWrapping::Wrap,
                .maxLines = 2,
            }
        );
        if (!tags.empty()) {
            contentChildren.push_back(
                Text {
                    .text = tags,
                    .font = Font::caption(),
                    .color = Color::tertiary(),
                    .horizontalAlignment = HorizontalAlignment::Leading,
                    .wrapping = TextWrapping::Wrap,
                    .maxLines = 2,
                }
            );
        }

        return ListRow {
            .content = VStack {
                .spacing = theme().space1,
                .alignment = Alignment::Start,
                .children = std::move(contentChildren)
            },
            .selected = selected,
            .onTap = onTap,
        };
    }
};

struct RemoteFileRow : ViewModifiers<RemoteFileRow> {
    RemoteModelFile file;
    bool downloading = false;
    std::size_t downloadedBytes = 0;
    std::size_t totalBytes = 0;
    std::function<void()> onDownload;
    std::function<void()> onCancel;

    auto body() const {
        auto theme = useEnvironmentReactive<ThemeKey>();
        bool const showProgress = downloading && totalBytes > 0;
        std::string meta = formatModelSize(file.sizeBytes) + (file.cached ? "  •  Cached locally" : "");
        if (downloading) {
            meta = shared_ui::formatTransferProgress(downloadedBytes, totalBytes);
        }

        std::vector<Element> fileInfo;
        fileInfo.reserve(showProgress ? 3 : 2);
        fileInfo.push_back(
            Text {
                .text = file.path,
                .font = Font::headline(),
                .color = Color::primary(),
                .horizontalAlignment = HorizontalAlignment::Leading,
                .wrapping = TextWrapping::Wrap,
            }
        );
        fileInfo.push_back(
            Text {
                .text = std::move(meta),
                .font = Font::footnote(),
                .color = Color::secondary(),
                .horizontalAlignment = HorizontalAlignment::Leading,
            }
        );
        if (showProgress) {
            fileInfo.push_back(
                ProgressBar {
                    .progress = static_cast<float>(downloadedBytes) / static_cast<float>(totalBytes),
                    .style = {
                        .activeColor = Color::accent(),
                        .inactiveColor = theme().hoveredControlBackgroundColor,
                        .trackHeight = 6.f,
                    },
                }.size(112.f, 0.f)
            );
        }

        return ListRow {
            .content = HStack {
                .spacing = theme().space3,
                .alignment = Alignment::Center,
                .children = children(
                    VStack {
                        .spacing = theme().space1,
                        .alignment = Alignment::Start,
                        .children = std::move(fileInfo)
                    }
                        .flex(1.f, 1.f),
                    LinkButton {
                        .label = file.cached ? "Cached" :
                                 downloading ? "Stop" :
                                               "Download",
                        .disabled = file.cached,
                        .onTap = downloading ? onCancel : onDownload,
                    }
                )
            },
        };
    }
};

struct HubView : ViewModifiers<HubView> {
    AppState state;
    std::function<void(std::string const &)> onSearchQueryChange;
    std::function<void(RemoteModelSort)> onSortChange;
    std::function<void(RemoteModelVisibilityFilter)> onVisibilityChange;
    std::function<void(std::string, RemoteModelSort, RemoteModelVisibilityFilter)> onSearch;
    std::function<void(std::string)> onSelectRemoteRepo;
    std::function<void(std::string, std::string)> onDownload;
    std::function<void(std::string const &)> onCancelDownload;

    auto body() const {
        auto theme = useEnvironmentReactive<ThemeKey>();
        auto searchQuery = useState<std::string>(state.modelSearchQuery);
        auto sortIndex = useState<int>(state.remoteModelSort == RemoteModelSort::Likes ? 1 :
                                       state.remoteModelSort == RemoteModelSort::Updated ? 2 :
                                                                                         0);
        auto visibilityIndex = useState<int>(state.remoteModelVisibility == RemoteModelVisibilityFilter::PublicOnly ? 1 :
                                             state.remoteModelVisibility == RemoteModelVisibilityFilter::GatedOnly ? 2 :
                                                                                                                     0);

        auto triggerSearch = [query = searchQuery,
                              sortIndex = sortIndex,
                              visibilityIndex = visibilityIndex,
                              onSearch = onSearch]() {
            if (onSearch) {
                onSearch(
                    *query,
                    *sortIndex == 1 ? RemoteModelSort::Likes :
                    *sortIndex == 2 ? RemoteModelSort::Updated :
                                      RemoteModelSort::Downloads,
                    *visibilityIndex == 1 ? RemoteModelVisibilityFilter::PublicOnly :
                    *visibilityIndex == 2 ? RemoteModelVisibilityFilter::GatedOnly :
                                            RemoteModelVisibilityFilter::All
                );
            }
        };

        RemoteModel const *selectedRemoteModel = nullptr;
        for (RemoteModel const &model : state.remoteModels) {
            if (model.id == state.selectedRemoteRepoId) {
                selectedRemoteModel = &model;
                break;
            }
        }
        RemoteRepoDetail const *selectedRemoteDetail =
            state.selectedRemoteRepoDetail.has_value() &&
                    state.selectedRemoteRepoDetail->id == state.selectedRemoteRepoId
                ? &*state.selectedRemoteRepoDetail
                : nullptr;

        std::vector<Element> remoteRows;
        remoteRows.reserve(state.remoteModels.size());
        for (RemoteModel const &model : state.remoteModels) {
            remoteRows.push_back(RemoteModelRow {
                .model = model,
                .selected = model.id == state.selectedRemoteRepoId,
                .onTap = [onSelectRemoteRepo = onSelectRemoteRepo, id = model.id] {
                    if (onSelectRemoteRepo) {
                        onSelectRemoteRepo(id);
                    }
                },
            });
        }

        std::vector<Element> fileRows;
        fileRows.reserve(state.selectedRemoteRepoFiles.size());
        for (RemoteModelFile const &file : state.selectedRemoteRepoFiles) {
            bool const isDownloading = state.downloadingModel && file.repoId == state.pendingDownloadRepoId &&
                                       file.path == state.pendingDownloadFilePath;
            std::size_t downloadedBytes = 0;
            std::size_t totalBytes = 0;
            if (isDownloading) {
                for (DownloadJob const &job : state.recentDownloadJobs) {
                    if (job.id != state.pendingDownloadJobId) {
                        continue;
                    }
                    downloadedBytes = job.downloadedBytes;
                    totalBytes = job.totalBytes;
                    break;
                }
            }
            fileRows.push_back(RemoteFileRow {
                .file = file,
                .downloading = isDownloading,
                .downloadedBytes = downloadedBytes,
                .totalBytes = totalBytes,
                .onDownload = [onDownload = onDownload, repoId = file.repoId, path = file.path] {
                    if (onDownload) {
                        onDownload(repoId, path);
                    }
                },
                .onCancel = [onCancelDownload = onCancelDownload, jobId = state.pendingDownloadJobId] {
                    if (onCancelDownload) {
                        onCancelDownload(jobId);
                    }
                },
            });
        }

        Element remoteResults = remoteRows.empty()
                                    ? Element {EmptyStatePanel {
                                          .title = state.searchingRemoteModels ? "Searching Hugging Face..." : "No models found",
                                          .detail = "Adjust the filters above and run a search to browse repositories with GGUF files.",
                                      }}
                                    : Element {ListView {.rows = std::move(remoteRows)}.flex(1.f, 1.f, 0.f)};

        Element remoteFiles = state.loadingRemoteModelFiles
                                  ? Element {EmptyStatePanel {
                                        .title = "Loading GGUF files...",
                                        .detail = "Resolving repository contents.",
                                    }}
                              : selectedRemoteModel == nullptr
                                  ? Element {Spacer {}.flex(1.f, 1.f)}
                              : fileRows.empty()
                                  ? Element {EmptyStatePanel {
                                        .title = "No GGUF files found",
                                        .detail = "The selected repository does not expose GGUF files.",
                                    }.flex(1.f, 1.f, 0.f)}
                                  : Element {ListView {.rows = std::move(fileRows)}.flex(1.f, 1.f, 0.f)};

        std::string selectedMeta;
        if (selectedRemoteModel != nullptr) {
            if (!selectedRemoteModel->author.empty()) {
                selectedMeta += selectedRemoteModel->author;
            }
            if (!selectedRemoteModel->libraryName.empty()) {
                if (!selectedMeta.empty()) {
                    selectedMeta += "  •  ";
                }
                selectedMeta += selectedRemoteModel->libraryName;
            }
            if (!selectedRemoteModel->pipelineTag.empty()) {
                if (!selectedMeta.empty()) {
                    selectedMeta += "  •  ";
                }
                selectedMeta += selectedRemoteModel->pipelineTag;
            }
            if (!selectedMeta.empty()) {
                selectedMeta += "  •  ";
            }
            selectedMeta += hub_view_detail::formatCompactCount(selectedRemoteModel->downloads) + " downloads";
        }

        std::vector<Element> selectedRepoChildren;
        if (selectedRemoteModel != nullptr) {
            selectedRepoChildren.push_back(
                Text {
                    .text = selectedRemoteModel->id,
                    .font = Font::headline(),
                    .color = Color::primary(),
                    .wrapping = TextWrapping::Wrap,
                }
            );
            if (!selectedMeta.empty()) {
                selectedRepoChildren.push_back(
                    Text {
                        .text = selectedMeta,
                        .font = Font::footnote(),
                        .color = Color::secondary(),
                        .wrapping = TextWrapping::Wrap,
                    }
                );
            }
            std::string selectedTags = joinedTags(selectedRemoteModel->tags, 5);
            if (!selectedTags.empty()) {
                selectedRepoChildren.push_back(
                    Text {
                        .text = std::move(selectedTags),
                        .font = Font::caption(),
                        .color = Color::tertiary(),
                        .wrapping = TextWrapping::Wrap,
                        .maxLines = 2,
                    }
                );
            }
            if (state.loadingRemoteRepoDetail && selectedRemoteDetail == nullptr) {
                selectedRepoChildren.push_back(
                    Text {
                        .text = "Loading repository details...",
                        .font = Font::footnote(),
                        .color = Color::secondary(),
                    }
                );
            } else if (selectedRemoteDetail != nullptr) {
                if (!selectedRemoteDetail->license.empty()) {
                    selectedRepoChildren.push_back(LabeledValueRow {
                        .label = "License",
                        .value = selectedRemoteDetail->license,
                    });
                }
                if (!selectedRemoteDetail->lastModified.empty()) {
                    selectedRepoChildren.push_back(LabeledValueRow {
                        .label = "Updated",
                        .value = selectedRemoteDetail->lastModified,
                        .maxLines = 1,
                    });
                }
                if (!selectedRemoteDetail->createdAt.empty()) {
                    selectedRepoChildren.push_back(LabeledValueRow {
                        .label = "Created",
                        .value = selectedRemoteDetail->createdAt,
                        .maxLines = 1,
                    });
                }
                if (!selectedRemoteDetail->sha.empty()) {
                    std::string sha = selectedRemoteDetail->sha.substr(0, std::min<std::size_t>(12, selectedRemoteDetail->sha.size()));
                    selectedRepoChildren.push_back(LabeledValueRow {
                        .label = "Revision",
                        .value = std::move(sha),
                        .maxLines = 1,
                    });
                }
                if (!selectedRemoteDetail->languages.empty()) {
                    selectedRepoChildren.push_back(
                        LabeledValueRow {
                            .label = "Languages",
                            .value = joinedTags(selectedRemoteDetail->languages, 4),
                        }
                    );
                }
                if (!selectedRemoteDetail->baseModels.empty()) {
                    selectedRepoChildren.push_back(
                        LabeledValueRow {
                            .label = "Base",
                            .value = joinedTags(selectedRemoteDetail->baseModels, 4),
                        }
                    );
                }
                if (!selectedRemoteDetail->summary.empty()) {
                    selectedRepoChildren.push_back(LabeledValueRow {
                        .label = "Summary",
                        .value = selectedRemoteDetail->summary,
                        .maxLines = 4,
                    });
                }
                if (!selectedRemoteDetail->readme.empty()) {
                    selectedRepoChildren.push_back(LabeledValueRow {
                        .label = "README",
                        .value = selectedRemoteDetail->readme,
                        .maxLines = 6,
                    });
                }
            }
        }

        return VStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                HStack {
                    .spacing = theme().space3,
                    .alignment = Alignment::Center,
                    .children = children(
                        TextInput {
                            .value = searchQuery,
                            .placeholder = "Search GGUF repositories",
                            .style = TextInput::Style {.height = 40.f},
                            .onChange = [onSearchQueryChange = onSearchQueryChange](std::string const &value) {
                                if (onSearchQueryChange) {
                                    onSearchQueryChange(value);
                                }
                            },
                            .onSubmit = [triggerSearch](std::string const &) {
                                triggerSearch();
                            },
                        }
                            .flex(1.8f, 1.f),
                        Select {
                            .selectedIndex = sortIndex,
                            .options = {
                                SelectOption {.label = "Downloads"},
                                SelectOption {.label = "Likes"},
                                SelectOption {.label = "Updated"},
                            },
                            .placeholder = "Sort",
                            .onChange = [onSortChange = onSortChange](int index) {
                                if (onSortChange) {
                                    onSortChange(
                                        index == 1 ? RemoteModelSort::Likes :
                                        index == 2 ? RemoteModelSort::Updated :
                                                     RemoteModelSort::Downloads
                                    );
                                }
                            },
                        }
                            .size(160.f, 40.f),
                        Select {
                            .selectedIndex = visibilityIndex,
                            .options = {
                                SelectOption {.label = "All"},
                                SelectOption {.label = "Public"},
                                SelectOption {.label = "Gated"},
                            },
                            .placeholder = "Visibility",
                            .onChange = [onVisibilityChange = onVisibilityChange](int index) {
                                if (onVisibilityChange) {
                                    onVisibilityChange(
                                        index == 1 ? RemoteModelVisibilityFilter::PublicOnly :
                                        index == 2 ? RemoteModelVisibilityFilter::GatedOnly :
                                                     RemoteModelVisibilityFilter::All
                                    );
                                }
                            },
                        }
                            .size(150.f, 40.f),
                        Button {
                            .label = state.searchingRemoteModels ? "Searching..." : "Search",
                            .disabled = state.searchingRemoteModels,
                            .onTap = triggerSearch,
                        }
                            .size(132.f, 40.f)
                    )
                }
                    .padding(theme().space4),
                Divider { .orientation = Divider::Orientation:: Horizontal },
                HStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        VStack {
                            .spacing = 0.f,
                            .alignment = Alignment::Stretch,
                            .children = children(std::move(remoteResults))
                        }
                            .fill(FillStyle::solid(Color::elevatedBackground()))
                            .size(420.f, 0.f),
                        Divider { .orientation = Divider::Orientation:: Vertical },
                        VStack {
                            .spacing = theme().space2,
                            .alignment = Alignment::Stretch,
                            .children = children(
                                selectedRemoteModel == nullptr ? Element {EmptyStatePanel {
                                                                     .title = "No repository selected",
                                                                     .detail = "Pick a search result to inspect its GGUF files.",
                                                                 }}
                                                               : Element {VStack {
                                                                     .spacing = theme().space1,
                                                                     .alignment = Alignment::Start,
                                                                     .children = std::move(selectedRepoChildren),
                                                                 }},
                                std::move(remoteFiles)
                            )
                        }
                            .padding(theme().space4)
                            .fill(FillStyle::solid(Color::elevatedBackground()))
                            .flex(1.f, 1.f)
                    )
                }
                    .flex(1.f, 1.f)
            )
        };
    }
};

} // namespace lambda
