#pragma once

#include <Flux.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "Divider.hpp"
#include "Types.hpp"

using namespace flux;

// ── Row for a local model ───────────────────────────────────────────────────

struct LocalModelRow : ViewModifiers<LocalModelRow> {
    LocalModelInfo model;
    bool active = false;
    std::function<void()> onTap;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        std::string label = model.displayName();
        std::string size;
        if (model.sizeBytes > 0) {
            if (model.sizeBytes >= 1024ULL * 1024 * 1024)
                size = std::to_string(model.sizeBytes / (1024 * 1024 * 1024)) + " GB";
            else if (model.sizeBytes >= 1024ULL * 1024)
                size = std::to_string(model.sizeBytes / (1024 * 1024)) + " MB";
        }

        auto handler = onTap;
        return HStack {
            .spacing = 8.f,
            .alignment = Alignment::Center,
            .children = children(
                VStack {
                    .spacing = 2.f,
                    .children = children(
                        Text {
                            .text = label,
                            .style = theme.typeBody,
                            .color = active ? theme.colorAccent : theme.colorTextPrimary,
                            .wrapping = TextWrapping::Wrap,
                        },
                        Text {
                            .text = size.empty() ? (model.tag.empty() ? std::string{} : model.tag) : size,
                            .style = theme.typeBodySmall,
                            .color = theme.colorTextSecondary,
                        }
                    )
                }.flex(1.f),
                active
                    ? Element { Icon {
                        .name = IconName::CheckCircle,
                        .size = 16.f,
                        .color = theme.colorAccent,
                    } }
                    : Element { Icon {
                        .name = IconName::Download,
                        .size = 16.f,
                        .color = theme.colorTextMuted,
                    } }
            )
        }
        .padding(8.f, 12.f, 8.f, 12.f)
        .cursor(Cursor::Hand)
        .onTap([handler]() { if (handler) handler(); });
    }
};

// ── Row for a HF search result ──────────────────────────────────────────────

struct HfModelRow : ViewModifiers<HfModelRow> {
    HfModelInfo model;
    std::function<void()> onTap;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        std::string downloads;
        if (model.downloads >= 1000000)
            downloads = std::to_string(model.downloads / 1000000) + "M";
        else if (model.downloads >= 1000)
            downloads = std::to_string(model.downloads / 1000) + "K";
        else
            downloads = std::to_string(model.downloads);

        auto handler = onTap;
        return VStack {
            .spacing = 2.f,
            .children = children(
                Text {
                    .text = model.id,
                    .style = theme.typeBody,
                    .color = theme.colorTextPrimary,
                    .wrapping = TextWrapping::Wrap,
                },
                HStack {
                    .spacing = 8.f,
                    .children = children(
                        Icon {
                            .name = IconName::Download,
                            .size = 12.f,
                            .color = theme.colorTextMuted,
                        },
                        Text {
                            .text = downloads,
                            .style = theme.typeBodySmall,
                            .color = theme.colorTextSecondary,
                        },
                        Text {
                            .text = model.pipelineTag,
                            .style = theme.typeBodySmall,
                            .color = theme.colorTextMuted,
                        }
                    )
                }
            )
        }
        .padding(8.f, 12.f, 8.f, 12.f)
        .cursor(Cursor::Hand)
        .onTap([handler]() { if (handler) handler(); });
    }
};

// ── Row for a GGUF file in a repo ───────────────────────────────────────────

struct HfFileRow : ViewModifiers<HfFileRow> {
    HfFileInfo file;
    std::function<void()> onDownload;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        std::string size;
        if (file.sizeBytes >= 1024ULL * 1024 * 1024) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1f GB", file.sizeBytes / (1024.0 * 1024.0 * 1024.0));
            size = buf;
        } else if (file.sizeBytes >= 1024ULL * 1024) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.0f MB", file.sizeBytes / (1024.0 * 1024.0));
            size = buf;
        }

        auto handler = onDownload;
        return HStack {
            .spacing = 8.f,
            .alignment = Alignment::Center,
            .children = children(
                Text {
                    .text = file.path,
                    .style = theme.typeBodySmall,
                    .color = theme.colorTextPrimary,
                    .wrapping = TextWrapping::Wrap,
                }.flex(1.f),
                Text {
                    .text = size,
                    .style = theme.typeBodySmall,
                    .color = theme.colorTextSecondary,
                },
                Icon {
                    .name = IconName::Download,
                    .size = 14.f,
                    .color = theme.colorAccent,
                }.cursor(Cursor::Hand)
                .onTap([handler]() { if (handler) handler(); })
            )
        }
        .padding(4.f, 16.f, 4.f, 16.f);
    }
};

// ── Main browser panel ──────────────────────────────────────────────────────

struct ModelBrowser : ViewModifiers<ModelBrowser> {
    std::vector<LocalModelInfo> localModels;
    std::vector<HfModelInfo>    hfResults;
    std::vector<HfFileInfo>     hfFiles;
    std::string                 activeModelPath;
    bool                        searching = false;
    bool                        downloading = false;

    std::function<void()>                                   onRefreshLocal;
    std::function<void(std::string const&)>                 onSearch;
    std::function<void(std::string const&)>                 onSelectRepo;
    std::function<void(std::string const&, std::string const&)> onDownload;
    std::function<void(std::string const&)>                 onLoadLocal;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        auto searchQuery = useState<std::string>("");
        auto expandedRepo = useState<std::string>("");
        auto tab = useState<int>(0);

        // ── Local models list ───────────────────────────────────────────
        std::vector<Element> localRows;
        for (auto const& m : localModels) {
            auto path = m.path;
            auto loadFn = onLoadLocal;
            bool isActive = (!path.empty() && path == activeModelPath);
            localRows.push_back(Element {
                LocalModelRow {
                    .model = m,
                    .active = isActive,
                    .onTap = [loadFn, path]() {
                        if (loadFn && !path.empty()) loadFn(path);
                    },
                }
            });
        }

        if (localRows.empty()) {
            localRows.push_back(Element {
                Text {
                    .text = "No local models found",
                    .style = theme.typeBodySmall,
                    .color = theme.colorTextMuted,
                }.padding(8.f, 12.f, 8.f, 12.f)
            });
        }

        // ── HF search results ───────────────────────────────────────────
        std::vector<Element> hfRows;
        std::string expanded = *expandedRepo;

        for (auto const& m : hfResults) {
            auto selectFn = onSelectRepo;
            auto downloadFn = onDownload;
            std::string repoId = m.id;
            bool isExpanded = (repoId == expanded);

            hfRows.push_back(Element {
                HfModelRow {
                    .model = m,
                    .onTap = [selectFn, expandedRepo, repoId]() {
                        expandedRepo = repoId;
                        if (selectFn) selectFn(repoId);
                    },
                }
            });

            if (isExpanded && !hfFiles.empty()) {
                for (auto const& f : hfFiles) {
                    std::string fPath = f.path;
                    std::string fRepo = f.repoId;
                    hfRows.push_back(Element {
                        HfFileRow {
                            .file = f,
                            .onDownload = [downloadFn, fRepo, fPath]() {
                                if (downloadFn) downloadFn(fRepo, fPath);
                            },
                        }
                    });
                }
            }
        }

        auto searchFn = onSearch;
        auto refreshFn = onRefreshLocal;
        int currentTab = *tab;

        // ── Tab headers ─────────────────────────────────────────────────
        auto tabBar = HStack {
            .spacing = 0.f,
            .children = children(
                Text {
                    .text = "Local",
                    .style = theme.typeLabel,
                    .color = currentTab == 0 ? theme.colorAccent : theme.colorTextSecondary,
                }
                .padding(8.f, 12.f, 8.f, 12.f)
                .cursor(Cursor::Hand)
                .onTap([tab]() { tab = 0; }),
                Text {
                    .text = "Hugging Face",
                    .style = theme.typeLabel,
                    .color = currentTab == 1 ? theme.colorAccent : theme.colorTextSecondary,
                }
                .padding(8.f, 12.f, 8.f, 12.f)
                .cursor(Cursor::Hand)
                .onTap([tab]() { tab = 1; })
            )
        };

        // ── Tab panels: both always mounted so TextInput hooks stay stable (Flux StateStore).
        //    Inactive panel uses opacity 0; active panel is stacked last so it receives hits.

        bool const onLocal = (currentTab == 0);
        float const localOp = onLocal ? 1.f : 0.f;
        float const hfOp    = onLocal ? 0.f : 1.f;

        Element localPanel = Element {
            VStack {
                .spacing = 0.f,
                .children = children(
                    HStack {
                        .spacing = 8.f,
                        .alignment = Alignment::Center,
                        .children = children(
                            Text {
                                .text = "Models",
                                .style = theme.typeLabel,
                                .color = theme.colorTextPrimary,
                            },
                            Spacer {},
                            Icon {
                                .name = IconName::Refresh,
                                .size = 16.f,
                                .color = theme.colorTextSecondary,
                            }
                            .cursor(Cursor::Hand)
                            .onTap([refreshFn]() { if (refreshFn) refreshFn(); })
                        )
                    }.padding(8.f, 12.f, 4.f, 12.f),
                    ScrollView {
                        .axis = ScrollAxis::Vertical,
                        .children = children(
                            VStack {
                                .spacing = 0.f,
                                .children = localRows,
                            }
                        )
                    }.flex(1.f)
                )
            }
            .flex(1.f)
            .opacity(localOp)
        };

        std::vector<Element> hfChildren;
        hfChildren.push_back(Element {
            HStack {
                .spacing = 0.f,
                .children = children(
                    TextInput {
                        .value = searchQuery,
                        .placeholder = "Search GGUF models...",
                        .disabled = onLocal,
                        .onSubmit = [searchFn](std::string const& q) {
                            if (searchFn) searchFn(q);
                        },
                    }.flex(1.f)
                )
            }.padding(8.f, 12.f, 4.f, 12.f)
        });
        if (downloading) {
            hfChildren.push_back(Element {
                Text {
                    .text = "Downloading...",
                    .style = theme.typeBodySmall,
                    .color = theme.colorAccent,
                }.padding(4.f, 12.f, 4.f, 12.f)
            });
        }
        hfChildren.push_back(Element {
            ScrollView {
                .axis = ScrollAxis::Vertical,
                .children = children(
                    VStack {
                        .spacing = 0.f,
                        .children = hfRows,
                    }
                )
            }.flex(1.f)
        });

        Element hfPanel = Element {
            VStack {
                .spacing = 0.f,
                .children = hfChildren,
            }
            .flex(1.f)
            .opacity(hfOp)
        };

        std::vector<Element> stacked = onLocal ? children(hfPanel, localPanel) : children(localPanel, hfPanel);

        std::vector<Element> content;
        content.push_back(Element{tabBar});
        content.push_back(Element{Divider{}});
        content.push_back(Element {
            ZStack {
                .horizontalAlignment = Alignment::Stretch,
                .verticalAlignment = Alignment::Stretch,
                .children = stacked,
            }.flex(1.f)
        });

        return VStack {
            .spacing = 0.f,
            .children = content,
        }.size(320.f, 0.f);
    }
};
