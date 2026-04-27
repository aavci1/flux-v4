#pragma once

#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <vector>

#include "AppState.hpp"
#include "SharedViews.hpp"

using namespace flux;

namespace lambda {

namespace model_row_presentation {

struct PresentedLocalModelRow {
    std::string title;
    std::string detail;
};

std::string lowercaseAscii(std::string text) {
    for (char &ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string uppercaseAscii(std::string text) {
    for (char &ch : text) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string lastPathComponent(std::string const &text) {
    std::size_t const slash = text.find_last_of('/');
    return slash == std::string::npos ? text : text.substr(slash + 1);
}

std::string trimKnownModelSuffixes(std::string text) {
    auto trimSuffix = [&text](std::string const &suffix) {
        if (text.size() < suffix.size()) {
            return false;
        }
        std::string const tail = lowercaseAscii(text.substr(text.size() - suffix.size()));
        if (tail != lowercaseAscii(suffix)) {
            return false;
        }
        text.resize(text.size() - suffix.size());
        return true;
    };
    trimSuffix(".gguf");
    trimSuffix("-gguf");
    return text;
}

std::vector<std::string> splitModelTokens(std::string const &text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char ch : text) {
        if (ch == '-' || ch == '/' || ch == ' ') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

bool tokenIsParameter(std::string const &token) {
    std::string const lower = lowercaseAscii(token);
    if (lower.size() < 2) {
        return false;
    }
    std::size_t index = 0;
    if ((lower[0] == 'e' || lower[0] == 'a') && lower.size() > 2) {
        index = 1;
    }
    bool seenDigit = false;
    bool seenDot = false;
    for (; index < lower.size(); ++index) {
        char const ch = lower[index];
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            seenDigit = true;
            continue;
        }
        if (ch == '.' && !seenDot) {
            seenDot = true;
            continue;
        }
        break;
    }
    return seenDigit && index == lower.size() - 1 && (lower[index] == 'b' || lower[index] == 'm');
}

bool tokenIsQuantization(std::string const &token) {
    std::string const lower = lowercaseAscii(token);
    if (lower == "f16" || lower == "bf16" || lower == "fp16" || lower == "f32" || lower == "fp32") {
        return true;
    }
    if (lower.size() >= 2 && lower[0] == 'q' && std::isdigit(static_cast<unsigned char>(lower[1]))) {
        return true;
    }
    if (lower.size() >= 3 && lower[0] == 'i' && lower[1] == 'q' &&
        std::isdigit(static_cast<unsigned char>(lower[2]))) {
        return true;
    }
    return false;
}

bool tokenIsIgnoredForTitle(std::string const &token) {
    std::string const lower = lowercaseAscii(token);
    return lower.empty() || lower == "gguf" || lower == "qat";
}

std::string joinNonEmpty(std::vector<std::string> const &parts, std::string const &separator) {
    std::string joined;
    for (std::string const &part : parts) {
        if (part.empty()) {
            continue;
        }
        if (!joined.empty()) {
            joined += separator;
        }
        joined += part;
    }
    return joined;
}

std::string humanizeModelWord(std::string word) {
    std::string lower = lowercaseAscii(word);
    if (lower == "it") {
        return "Instruct";
    }
    if (lower == "gguf") {
        return "GGUF";
    }
    bool const alphaOnly = std::all_of(word.begin(), word.end(), [](char ch) {
        return std::isalpha(static_cast<unsigned char>(ch)) != 0;
    });
    if (alphaOnly && word.size() <= 3) {
        return uppercaseAscii(lower);
    }
    if (!word.empty()) {
        word = lowercaseAscii(word);
        word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
    }
    return word;
}

std::string humanizeModelTitleToken(std::string const &token) {
    std::string expanded;
    expanded.reserve(token.size() + 4);
    for (std::size_t i = 0; i < token.size(); ++i) {
        char const ch = token[i];
        if (ch == '_') {
            expanded.push_back(' ');
            continue;
        }
        if (i > 0) {
            char const prev = token[i - 1];
            if (std::isalpha(static_cast<unsigned char>(prev)) &&
                std::isdigit(static_cast<unsigned char>(ch))) {
                expanded.push_back(' ');
            }
        }
        expanded.push_back(ch);
    }

    std::vector<std::string> words = splitModelTokens(expanded);
    std::string result;
    for (std::string const &word : words) {
        if (word.empty()) {
            continue;
        }
        if (!result.empty()) {
            result += ' ';
        }
        result += humanizeModelWord(word);
    }
    return result;
}

PresentedLocalModelRow presentLocalModel(LocalModel const &model) {
    std::string const fileStem = trimKnownModelSuffixes(lastPathComponent(model.path));
    std::string const repoStem = trimKnownModelSuffixes(lastPathComponent(model.repo));
    std::vector<std::string> const fileTokens = splitModelTokens(fileStem);
    std::vector<std::string> const titleSourceTokens =
        !repoStem.empty() ? splitModelTokens(repoStem)
                          : (!model.name.empty() ? splitModelTokens(trimKnownModelSuffixes(model.name))
                                                 : fileTokens);

    std::vector<std::string> titleParts;
    titleParts.reserve(titleSourceTokens.size());
    for (std::string const &token : titleSourceTokens) {
        if (tokenIsIgnoredForTitle(token) || tokenIsParameter(token) || tokenIsQuantization(token)) {
            continue;
        }
        titleParts.push_back(humanizeModelTitleToken(token));
    }

    if (titleParts.empty()) {
        for (std::string const &token : fileTokens) {
            if (tokenIsIgnoredForTitle(token) || tokenIsParameter(token) || tokenIsQuantization(token)) {
                continue;
            }
            titleParts.push_back(humanizeModelTitleToken(token));
        }
    }

    std::vector<std::string> parameterParts;
    std::vector<std::string> quantParts;
    for (std::string const &token : fileTokens) {
        if (tokenIsParameter(token)) {
            parameterParts.push_back(uppercaseAscii(token));
        } else if (tokenIsQuantization(token)) {
            quantParts.push_back(uppercaseAscii(token));
        }
    }

    std::vector<std::string> detailParts;
    if (!parameterParts.empty()) {
        detailParts.push_back(joinNonEmpty(parameterParts, " / "));
    }
    if (!quantParts.empty()) {
        detailParts.push_back(joinNonEmpty(quantParts, " / "));
    }
    if (model.sizeBytes > 0) {
        detailParts.push_back(formatModelSize(model.sizeBytes));
    }

    return PresentedLocalModelRow {
        .title = joinNonEmpty(titleParts, " "),
        .detail = joinNonEmpty(detailParts, "  •  "),
    };
}

} // namespace model_row_presentation

struct ModelRow : ViewModifiers<ModelRow> {
    LocalModel model;
    bool active = false;
    bool loading = false;
    bool deleting = false;
    std::function<void()> onLoad;
    std::function<void()> onDelete;

    auto body() const {
        auto theme = useEnvironment<Theme>();
        model_row_presentation::PresentedLocalModelRow const presented =
            model_row_presentation::presentLocalModel(model);

        return ListRow {
            .content = HStack {
                .spacing = theme().space3,
                .alignment = Alignment::Center,
                .children = children(
                    VStack {
                        .spacing = theme().space1,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text {
                                .text = presented.title,
                                .font = Font::headline(),
                                .color = active ? Color::accent() : Color::primary(),
                                .horizontalAlignment = HorizontalAlignment::Leading,
                            },
                            Text {
                                .text = presented.detail,
                                .font = Font::footnote(),
                                .color = Color::secondary(),
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
                            .size = theme().titleFont.size,
                            .weight = theme().headlineFont.weight,
                            .color = deleting ? Color::disabled() : Color::danger(),
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
        auto theme = useEnvironment<Theme>();
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
                .font = Font::caption(),
                .color = job.status == DownloadJobStatus::Failed ? Color::danger() : Color::primary(),
                .wrapping = TextWrapping::Wrap,
                .maxLines = 2,
            }
        );
        infoChildren.push_back(
            Text {
                .text = meta,
                .font = Font::footnote(),
                .color = Color::secondary(),
                .wrapping = TextWrapping::Wrap,
                .maxLines = 2,
            }
        );
        if (job.status == DownloadJobStatus::Running && job.totalBytes > 0) {
            infoChildren.push_back(
                ProgressBar {
                    .progress = downloadJobProgress(job),
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
                        .spacing = theme().space2,
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
                                    .spacing = theme().space2,
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
                                                .size = theme().bodyFont.size,
                                                .weight = theme().headlineFont.weight,
                                                .color = Color::secondary(),
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
        auto theme = useEnvironment<Theme>();

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
                    .spacing = theme().space3,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = "Models",
                            .font = Font::title(),
                            .color = Color::primary(),
                        }
                            .flex(1.f, 1.f),
                        LinkButton {
                            .label = state.refreshingModels ? "Refreshing..." : "Refresh",
                            .disabled = state.refreshingModels,
                            .onTap = onRefresh,
                        }
                    )
                }
                    .padding(theme().space4),
                Divider {
                    .orientation = Divider::Orientation::Horizontal
                },
                std::move(content)
            )
        }
            .fill(FillStyle::solid(Color::elevatedBackground()));
    }
};

} // namespace lambda
