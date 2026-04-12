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

namespace {

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

} // namespace

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
        meta += "  •  " + formatCompactCount(model.downloads) + " downloads";
        meta += "  •  " + formatCompactCount(model.likes) + " likes";

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
    std::function<void()> onDownload;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

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
                                .text = file.path,
                                .font = theme.fontLabel,
                                .color = theme.colorTextPrimary,
                                .horizontalAlignment = HorizontalAlignment::Leading,
                                .wrapping = TextWrapping::Wrap,
                            },
                            Text {
                                .text = formatModelSize(file.sizeBytes) +
                                        (file.cached ? "  •  Cached locally" : ""),
                                .font = theme.fontBodySmall,
                                .color = theme.colorTextSecondary,
                                .horizontalAlignment = HorizontalAlignment::Leading,
                            }
                        )
                    }
                        .flex(1.f, 1.f),
                    LinkButton {
                        .label = file.cached ? "Cached" : downloading ? "Downloading..." : "Download",
                        .disabled = file.cached || downloading,
                        .onTap = onDownload,
                    }
                )
            },
        };
    }
};

struct ModelsView : ViewModifiers<ModelsView> {
    AppState state;
    std::function<void()> onRefresh;
    std::function<void(std::string const &, std::string const &)> onLoad;
    std::function<void(std::string const &)> onSearchQueryChange;
    std::function<void(std::string)> onSearch;
    std::function<void(std::string)> onSelectRemoteRepo;
    std::function<void(std::string, std::string)> onDownload;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto searchQuery = useState<std::string>(state.modelSearchQuery);

        auto triggerSearch = [query = searchQuery, onSearch = onSearch]() {
            if (onSearch) {
                onSearch(*query);
            }
        };

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

        RemoteModel const *selectedRemoteModel = nullptr;
        for (RemoteModel const &model : state.remoteModels) {
            if (model.id == state.selectedRemoteRepoId) {
                selectedRemoteModel = &model;
                break;
            }
        }

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
            fileRows.push_back(RemoteFileRow {
                .file = file,
                .downloading = isDownloading,
                .onDownload = [onDownload = onDownload, repoId = file.repoId, path = file.path] {
                    if (onDownload) {
                        onDownload(repoId, path);
                    }
                },
            });
        }

        Element localContent = localRows.empty()
                                   ? placeholderPanel(
                                         theme,
                                         "No local models found",
                                         "Lambda Studio now indexes the Hugging Face cache first. You can also place `.gguf` files in `~/.lambda-studio/models`."
                                     )
                                   : Element {ListView {.rows = std::move(localRows)}.flex(1.f, 1.f, 0.f)};

        Element remoteResults = remoteRows.empty()
                                    ? placeholderPanel(
                                          theme,
                                          state.searchingRemoteModels ? "Searching Hugging Face..." :
                                                                        "Search Hugging Face",
                                          "Enter a model name or keyword to browse GGUF repositories and available files."
                                      )
                                    : Element {ListView {.rows = std::move(remoteRows)}.size(0.f, 240.f)};

        Element remoteFiles = state.loadingRemoteModelFiles
                                  ? placeholderPanel(theme, "Loading GGUF files...", "Resolving repository contents.")
                              : selectedRemoteModel == nullptr
                                  ? Element {Spacer {}.flex(1.f, 1.f)}
                              : fileRows.empty()
                                  ? placeholderPanel(theme, "No GGUF files found", "The selected repository does not expose GGUF files.")
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
            selectedMeta += formatCompactCount(selectedRemoteModel->downloads) + " downloads";
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
        }

        return HStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                VStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        HStack {
                            .spacing = theme.space3,
                            .alignment = Alignment::Center,
                            .children = children(
                                Text {
                                    .text = "Installed",
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
                        std::move(localContent)
                    )
                }
                    .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                    .size(360.f, 0.f),
                Rectangle {}
                    .size(1.f, 0.f)
                    .fill(FillStyle::solid(theme.colorBorderSubtle)),
                VStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        VStack {
                            .spacing = theme.space3,
                            .alignment = Alignment::Stretch,
                            .children = children(
                                HStack {
                                    .spacing = theme.space3,
                                    .alignment = Alignment::Center,
                                    .children = children(
                                        Text {
                                            .text = "Explore Hugging Face",
                                            .font = theme.fontHeading,
                                            .color = theme.colorTextPrimary,
                                        }
                                            .flex(1.f, 1.f),
                                        LinkButton {
                                            .label = state.searchingRemoteModels ? "Searching..." : "Search",
                                            .disabled = state.searchingRemoteModels,
                                            .onTap = triggerSearch,
                                        }
                                    )
                                },
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
                                },
                                std::move(remoteResults)
                            )
                        }
                            .padding(theme.space4),
                        Rectangle {}
                            .size(0.f, 1.f)
                            .fill(FillStyle::solid(theme.colorBorderSubtle)),
                        VStack {
                            .spacing = theme.space2,
                            .alignment = Alignment::Stretch,
                            .children = children(
                                selectedRemoteModel == nullptr ? placeholderPanel(theme, "No repository selected", "Pick a search result to inspect its GGUF files.")
                                                               : Element {VStack {
                                                                     .spacing = theme.space1,
                                                                     .alignment = Alignment::Start,
                                                                     .children = std::move(selectedRepoChildren),
                                                                 }},
                                std::move(remoteFiles)
                            )
                        }
                            .padding(theme.space4)
                            .flex(1.f, 1.f)
                    )
                }
                    .fill(FillStyle::solid(theme.colorSurfaceOverlay))
                    .flex(1.f, 1.f)
            )
        };
    }
};

} // namespace lambda
