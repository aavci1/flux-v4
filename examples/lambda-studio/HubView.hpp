#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "AppState.hpp"

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

inline Element detailLine(
    Theme const &theme,
    std::string label,
    std::string value,
    int maxLines = 2
) {
    return HStack {
        .spacing = theme.space2,
        .alignment = Alignment::Start,
        .children = children(
            Text {
                .text = std::move(label),
                .font = theme.fontLabelSmall,
                .color = theme.colorTextMuted,
            }
                .size(96.f, 0.f),
            Text {
                .text = std::move(value),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .wrapping = TextWrapping::Wrap,
                .maxLines = maxLines,
            }
                .flex(1.f, 1.f)
        )
    };
}

} // namespace hub_view_detail

struct RemoteModelRow : ViewModifiers<RemoteModelRow> {
    RemoteModel model;
    bool selected = false;
    std::function<void()> onTap;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

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
                .font = theme.fontLabel,
                .color = selected ? theme.colorAccent : theme.colorTextPrimary,
                .horizontalAlignment = HorizontalAlignment::Leading,
                .wrapping = TextWrapping::Wrap,
            }
        );
        contentChildren.push_back(
            Text {
                .text = std::move(meta),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .horizontalAlignment = HorizontalAlignment::Leading,
                .wrapping = TextWrapping::Wrap,
                .maxLines = 2,
            }
        );
        if (!tags.empty()) {
            contentChildren.push_back(
                Text {
                    .text = tags,
                    .font = theme.fontLabelSmall,
                    .color = theme.colorTextMuted,
                    .horizontalAlignment = HorizontalAlignment::Leading,
                    .wrapping = TextWrapping::Wrap,
                    .maxLines = 2,
                }
            );
        }

        return ListRow {
            .content = VStack {
                .spacing = theme.space1,
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

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        bool const showProgress = downloading && totalBytes > 0;
        std::string meta = formatModelSize(file.sizeBytes) + (file.cached ? "  •  Cached locally" : "");
        if (downloading) {
            meta = hub_view_detail::formatTransferProgress(downloadedBytes, totalBytes);
        }

        std::vector<Element> fileInfo;
        fileInfo.reserve(showProgress ? 3 : 2);
        fileInfo.push_back(
            Text {
                .text = file.path,
                .font = theme.fontLabel,
                .color = theme.colorTextPrimary,
                .horizontalAlignment = HorizontalAlignment::Leading,
                .wrapping = TextWrapping::Wrap,
            }
        );
        fileInfo.push_back(
            Text {
                .text = std::move(meta),
                .font = theme.fontBodySmall,
                .color = theme.colorTextSecondary,
                .horizontalAlignment = HorizontalAlignment::Leading,
            }
        );
        if (showProgress) {
            fileInfo.push_back(hub_view_detail::progressBar(theme, static_cast<float>(downloadedBytes) / static_cast<float>(totalBytes)));
        }

        return ListRow {
            .content = HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Center,
                .children = children(
                    VStack {
                        .spacing = theme.space1,
                        .alignment = Alignment::Start,
                        .children = std::move(fileInfo)
                    }
                        .flex(1.f, 1.f),
                    LinkButton {
                        .label = file.cached ? "Cached" :
                                 downloading ? (totalBytes == 0 ? "..." :
                                                std::to_string(hub_view_detail::progressPercent(downloadedBytes, totalBytes)) + "%") :
                                               "Download",
                        .disabled = file.cached || downloading,
                        .onTap = onDownload,
                    }
                )
            },
        };
    }
};

struct HubView : ViewModifiers<HubView> {
    AppState state;
    std::function<void(std::string const &)> onSearchQueryChange;
    std::function<void(std::string const &)> onSearchAuthorChange;
    std::function<void(RemoteModelSort)> onSortChange;
    std::function<void(RemoteModelVisibilityFilter)> onVisibilityChange;
    std::function<void(std::string, std::string, RemoteModelSort, RemoteModelVisibilityFilter)> onSearch;
    std::function<void(std::string)> onSelectRemoteRepo;
    std::function<void(std::string, std::string)> onDownload;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto searchQuery = useState<std::string>(state.modelSearchQuery);
        auto searchAuthor = useState<std::string>(state.modelSearchAuthor);
        auto sortIndex = useState<int>(state.remoteModelSort == RemoteModelSort::Likes ? 1 :
                                       state.remoteModelSort == RemoteModelSort::Updated ? 2 :
                                                                                         0);
        auto visibilityIndex = useState<int>(state.remoteModelVisibility == RemoteModelVisibilityFilter::PublicOnly ? 1 :
                                             state.remoteModelVisibility == RemoteModelVisibilityFilter::GatedOnly ? 2 :
                                                                                                                     0);

        auto triggerSearch = [query = searchQuery,
                              author = searchAuthor,
                              sortIndex = sortIndex,
                              visibilityIndex = visibilityIndex,
                              onSearch = onSearch]() {
            if (onSearch) {
                onSearch(
                    *query,
                    *author,
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
            });
        }

        Element remoteResults = remoteRows.empty()
                                    ? hub_view_detail::placeholderPanel(
                                          theme,
                                          state.searchingRemoteModels ? "Searching Hugging Face..." :
                                                                        "No models found",
                                          "Adjust the filters above and run a search to browse GGUF repositories."
                                      )
                                    : Element {ListView {.rows = std::move(remoteRows)}.flex(1.f, 1.f, 0.f)};

        Element remoteFiles = state.loadingRemoteModelFiles
                                  ? hub_view_detail::placeholderPanel(theme, "Loading GGUF files...", "Resolving repository contents.")
                              : selectedRemoteModel == nullptr
                                  ? Element {Spacer {}.flex(1.f, 1.f)}
                              : fileRows.empty()
                                  ? hub_view_detail::placeholderPanel(theme, "No GGUF files found", "The selected repository does not expose GGUF files.")
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
                    .font = theme.fontLabel,
                    .color = theme.colorTextPrimary,
                    .wrapping = TextWrapping::Wrap,
                }
            );
            if (!selectedMeta.empty()) {
                selectedRepoChildren.push_back(
                    Text {
                        .text = selectedMeta,
                        .font = theme.fontBodySmall,
                        .color = theme.colorTextSecondary,
                        .wrapping = TextWrapping::Wrap,
                    }
                );
            }
            std::string selectedTags = joinedTags(selectedRemoteModel->tags, 5);
            if (!selectedTags.empty()) {
                selectedRepoChildren.push_back(
                    Text {
                        .text = std::move(selectedTags),
                        .font = theme.fontLabelSmall,
                        .color = theme.colorTextMuted,
                        .wrapping = TextWrapping::Wrap,
                        .maxLines = 2,
                    }
                );
            }
            if (state.loadingRemoteRepoDetail && selectedRemoteDetail == nullptr) {
                selectedRepoChildren.push_back(
                    Text {
                        .text = "Loading repository details...",
                        .font = theme.fontBodySmall,
                        .color = theme.colorTextSecondary,
                    }
                );
            } else if (selectedRemoteDetail != nullptr) {
                if (!selectedRemoteDetail->license.empty()) {
                    selectedRepoChildren.push_back(hub_view_detail::detailLine(theme, "License", selectedRemoteDetail->license));
                }
                if (!selectedRemoteDetail->lastModified.empty()) {
                    selectedRepoChildren.push_back(hub_view_detail::detailLine(theme, "Updated", selectedRemoteDetail->lastModified, 1));
                }
                if (!selectedRemoteDetail->createdAt.empty()) {
                    selectedRepoChildren.push_back(hub_view_detail::detailLine(theme, "Created", selectedRemoteDetail->createdAt, 1));
                }
                if (!selectedRemoteDetail->sha.empty()) {
                    std::string sha = selectedRemoteDetail->sha.substr(0, std::min<std::size_t>(12, selectedRemoteDetail->sha.size()));
                    selectedRepoChildren.push_back(hub_view_detail::detailLine(theme, "Revision", sha, 1));
                }
                if (!selectedRemoteDetail->languages.empty()) {
                    selectedRepoChildren.push_back(
                        hub_view_detail::detailLine(theme, "Languages", joinedTags(selectedRemoteDetail->languages, 4), 2)
                    );
                }
                if (!selectedRemoteDetail->baseModels.empty()) {
                    selectedRepoChildren.push_back(
                        hub_view_detail::detailLine(theme, "Base", joinedTags(selectedRemoteDetail->baseModels, 4), 2)
                    );
                }
                if (!selectedRemoteDetail->summary.empty()) {
                    selectedRepoChildren.push_back(hub_view_detail::detailLine(theme, "Summary", selectedRemoteDetail->summary, 4));
                }
                if (!selectedRemoteDetail->readme.empty()) {
                    selectedRepoChildren.push_back(hub_view_detail::detailLine(theme, "README", selectedRemoteDetail->readme, 6));
                }
            }
        }

        return VStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                HStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Center,
                    .children = children(
                        TextInput {
                            .value = searchQuery,
                            .placeholder = "Search model repos",
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
                            .flex(1.4f, 1.f),
                        TextInput {
                            .value = searchAuthor,
                            .placeholder = "Author or org",
                            .style = TextInput::Style {.height = 40.f},
                            .onChange = [onSearchAuthorChange = onSearchAuthorChange](std::string const &value) {
                                if (onSearchAuthorChange) {
                                    onSearchAuthorChange(value);
                                }
                            },
                            .onSubmit = [triggerSearch](std::string const &) {
                                triggerSearch();
                            },
                        }
                            .flex(1.f, 1.f),
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
                    .padding(theme.space4),
                Rectangle {}
                    .size(0.f, 1.f)
                    .fill(FillStyle::solid(theme.colorBorderSubtle)),
                HStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        VStack {
                            .spacing = 0.f,
                            .alignment = Alignment::Stretch,
                            .children = children(std::move(remoteResults))
                        }
                            .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                            .size(420.f, 0.f),
                        Rectangle {}
                            .size(1.f, 0.f)
                            .fill(FillStyle::solid(theme.colorBorderSubtle)),
                        VStack {
                            .spacing = theme.space2,
                            .alignment = Alignment::Stretch,
                            .children = children(
                                selectedRemoteModel == nullptr ? hub_view_detail::placeholderPanel(theme, "No repository selected", "Pick a search result to inspect its GGUF files.")
                                                               : Element {VStack {
                                                                     .spacing = theme.space1,
                                                                     .alignment = Alignment::Start,
                                                                     .children = std::move(selectedRepoChildren),
                                                                 }},
                                std::move(remoteFiles)
                            )
                        }
                            .padding(theme.space4)
                            .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                            .flex(1.f, 1.f)
                    )
                }
                    .flex(1.f, 1.f)
            )
        };
    }
};

} // namespace lambda
