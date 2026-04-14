#pragma once

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "AppState.hpp"
#include "ChatModels.hpp"
#include "Debug.hpp"
#include "MarkdownText.hpp"

using namespace flux;

namespace lambda {

namespace {

struct PresentedLocalModel {
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

PresentedLocalModel presentLocalModel(LocalModel const &model) {
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

    return PresentedLocalModel{
        .title = joinNonEmpty(titleParts, " "),
        .detail = joinNonEmpty(detailParts, "  •  "),
    };
}

std::string formatThoughtDuration(std::int64_t startedAtNanos, std::int64_t finishedAtNanos) {
    if (startedAtNanos <= 0 || finishedAtNanos <= startedAtNanos) {
        return "Thought completed";
    }

    std::int64_t const totalSeconds = std::max<std::int64_t>(1, (finishedAtNanos - startedAtNanos) / 1'000'000'000LL);
    std::int64_t const hours = totalSeconds / 3600;
    std::int64_t const minutes = (totalSeconds % 3600) / 60;
    std::int64_t const seconds = totalSeconds % 60;

    if (hours > 0) {
        return "Thought for " + std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    if (minutes > 0) {
        if (seconds == 0) {
            return "Thought for " + std::to_string(minutes) + "m";
        }
        return "Thought for " + std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
    }
    return "Thought for " + std::to_string(totalSeconds) + "s";
}

std::string formatMsLabel(std::int64_t millis) {
    if (millis <= 0) {
        return {};
    }
    if (millis < 1000) {
        return std::to_string(millis) + " ms";
    }

    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(1);
    stream << static_cast<double>(millis) / 1000.0 << " s";
    return stream.str();
}

std::string formatRateLabel(double value, std::string const &suffix) {
    if (value <= 0.0) {
        return {};
    }
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(value >= 100.0 ? 0 : 1);
    stream << value << ' ' << suffix;
    return stream.str();
}

std::string compactModelLabel(MessageGenerationStats const &stats) {
    if (!stats.modelName.empty()) {
        return stats.modelName;
    }
    if (!stats.modelPath.empty()) {
        return trimKnownModelSuffixes(lastPathComponent(stats.modelPath));
    }
    return {};
}

std::string generationStatusLabel(std::string const &status) {
    if (status == "completed") {
        return "Complete";
    }
    if (status == "cancelled") {
        return "Stopped";
    }
    if (status == "max_tokens") {
        return "Max tokens";
    }
    if (status == "error") {
        return "Error";
    }
    return status;
}

std::vector<std::string> generationStatsPrimaryParts(MessageGenerationStats const &stats) {
    std::vector<std::string> parts;
    if (std::string model = compactModelLabel(stats); !model.empty()) {
        parts.push_back(model);
    }
    if (stats.completionTokens > 0) {
        parts.push_back(std::to_string(stats.completionTokens) + " tok");
    }
    if (std::string tps = formatRateLabel(stats.tokensPerSecond, "tok/s"); !tps.empty()) {
        parts.push_back(tps);
    }
    if (stats.firstTokenAtUnixMs > stats.startedAtUnixMs) {
        parts.push_back("TTFT " + formatMsLabel(stats.firstTokenAtUnixMs - stats.startedAtUnixMs));
    }
    return parts;
}

} // namespace

struct ThinkingDots : ViewModifiers<ThinkingDots> {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        Transition const trInstant = Transition::instant();
        Transition const trLoop = theme.reducedMotion ? trInstant : Transition::linear(0.66f);

        auto phase = useAnimated<float>(0.f);
        if (!theme.reducedMotion && !phase.animated->isAnimating()) {
            if (*phase >= 2.999f) {
                phase.set(0.f, trInstant);
            }
            phase.set(3.f, trLoop);
        }

        float const wrapped = theme.reducedMotion ? 0.f : std::fmod(std::max(0.f, *phase), 3.f);

        std::vector<Element> dots;
        dots.reserve(3);
        for (int i = 0; i < 3; ++i) {
            float const center = static_cast<float>(i);
            float const delta = std::abs(wrapped - center);
            float const distance = std::min(delta, 3.f - delta);
            float const emphasis = theme.reducedMotion ? (i == 0 ? 1.f : 0.f) : std::clamp(1.f - distance, 0.f, 1.f);
            dots.push_back(
                Rectangle {}
                    .size(8.f, 8.f)
                    .cornerRadius(4.f)
                    .fill(FillStyle::solid(theme.colorTextSecondary))
                    .opacity(0.28f + emphasis * 0.72f)
                    .position(0.f, (emphasis - 0.5f) * 4.f)
            );
        }

        return HStack {
            .spacing = theme.space1,
            .alignment = Alignment::Center,
            .children = std::move(dots),
        }
            .size(30.f, 12.f);
    }
};

struct ChatBubble : ViewModifiers<ChatBubble> {
    ChatMessage message;
    bool deferTailMarkdown = false;
    std::function<void()> onToggleReasoning;
    std::function<void()> onDeleteMessage;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        bool const isUser = message.role == ChatRole::User;
        bool const isReasoning = message.role == ChatRole::Reasoning;
        bool const hovered = useHover();
        bool const pressed = usePress();
        bool const collapsed = isReasoning && message.collapsed;
        bool const reasoningFinished = message.finishedAtNanos > message.startedAtNanos;
        std::string const thoughtSummary = formatThoughtDuration(message.startedAtNanos, message.finishedAtNanos);
        bool const hasDeleteAction = static_cast<bool>(onDeleteMessage);
        bool const showSummaryFooter = !collapsed && (message.generationStats.has_value() || hasDeleteAction);

        Color const fill = isUser      ? theme.colorAccent :
                           isReasoning ? theme.colorSurface :
                                         theme.colorSurfaceOverlay;
        Color const textColor = isUser ? theme.colorTextOnAccent : theme.colorTextPrimary;
        Color const summaryColor = isUser ? theme.colorTextOnAccent : theme.colorTextMuted;
        Color const deleteIconColor = isUser ? theme.colorTextOnAccent : theme.colorTextSecondary;
        MarkdownResolvedStyle const markdownStyle =
            resolveMarkdownStyle(theme, isReasoning ? theme.fontBodySmall : theme.fontBody, textColor);
        auto buildSummaryRow = [&](std::string primaryLine) -> Element {
            std::vector<Element> summaryChildren;
            if (!primaryLine.empty()) {
                summaryChildren.push_back(
                    Text {
                        .text = primaryLine,
                        .font = theme.fontLabelSmall,
                        .color = summaryColor,
                        .horizontalAlignment = HorizontalAlignment::Leading,
                        .wrapping = TextWrapping::Wrap,
                    }
                        .flex(1.f, 1.f)
                );
            } else {
                summaryChildren.push_back(Spacer {});
            }
            if (hasDeleteAction) {
                summaryChildren.push_back(
                    IconButton {
                        .icon = IconName::Delete,
                        .style = {
                            .size = 14.f,
                            .weight = 500.f,
                            .color = deleteIconColor,
                        },
                        .onTap = onDeleteMessage,
                    }
                );
            }
            return Element {
                HStack {
                    .spacing = theme.space2,
                    .alignment = Alignment::Center,
                    .children = std::move(summaryChildren),
                }
            };
        };

        if (!isUser && !isReasoning) {
            std::vector<Element> paragraphs;
            if (message.paragraphs.empty()) {
                paragraphs.push_back(
                    (deferTailMarkdown ? Element {
                                             Text {
                                                 .text = message.text,
                                                 .font = markdownStyle.baseFont,
                                                 .color = textColor,
                                                 .horizontalAlignment = HorizontalAlignment::Leading,
                                                 .verticalAlignment = VerticalAlignment::Top,
                                                 .wrapping = TextWrapping::Wrap,
                                             }
                                         } :
                                         Element {
                                             MarkdownText {
                                                 .text = &message.text,
                                                 .cacheKey = message.renderKey,
                                                 .textRevision = message.textRevision,
                                                 .baseFont = markdownStyle.baseFont,
                                                 .codeFont = markdownStyle.codeFont,
                                                 .h1Font = markdownStyle.h1Font,
                                                 .h2Font = markdownStyle.h2Font,
                                                 .h3Font = markdownStyle.h3Font,
                                                 .baseColor = markdownStyle.baseColor,
                                                 .codeBackground = markdownStyle.codeBackground,
                                                 .horizontalAlignment = HorizontalAlignment::Leading,
                                                 .verticalAlignment = VerticalAlignment::Top,
                                                 .wrapping = TextWrapping::Wrap,
                                             }
                                         })
                );
            } else {
                paragraphs.reserve(message.paragraphs.size());
                for (std::size_t i = 0; i < message.paragraphs.size(); ++i) {
                    ChatMessage::Paragraph const &paragraph = message.paragraphs[i];
                    bool const renderAsPlainText = deferTailMarkdown && i + 1 == message.paragraphs.size();
                    paragraphs.push_back(
                        renderAsPlainText ? Element {
                                                Text {
                                                    .text = paragraph.text,
                                                    .font = markdownStyle.baseFont,
                                                    .color = textColor,
                                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                                    .verticalAlignment = VerticalAlignment::Top,
                                                    .wrapping = TextWrapping::Wrap,
                                                }
                                            } :
                                            Element {
                                                MarkdownText {
                                                    .text = &paragraph.text,
                                                    .cacheKey = paragraph.renderKey,
                                                    .textRevision = paragraph.textRevision,
                                                    .baseFont = markdownStyle.baseFont,
                                                    .codeFont = markdownStyle.codeFont,
                                                    .h1Font = markdownStyle.h1Font,
                                                    .h2Font = markdownStyle.h2Font,
                                                    .h3Font = markdownStyle.h3Font,
                                                    .baseColor = markdownStyle.baseColor,
                                                    .codeBackground = markdownStyle.codeBackground,
                                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                                    .verticalAlignment = VerticalAlignment::Top,
                                                    .wrapping = TextWrapping::Wrap,
                                                }
                                            }
                    );
                }
            }

            if (showSummaryFooter) {
                std::string primaryLine;
                if (message.generationStats.has_value()) {
                    primaryLine = joinNonEmpty(generationStatsPrimaryParts(*message.generationStats), "  •  ");
                }
                std::vector<Element> footerChildren;
                footerChildren.push_back(
                    Element { Divider { .orientation = Divider::Orientation::Horizontal } }
                );
                footerChildren.push_back(buildSummaryRow(std::move(primaryLine)));
                paragraphs.push_back(
                    VStack {
                        .spacing = theme.space2,
                        .alignment = Alignment::Start,
                        .children = std::move(footerChildren),
                    }
                        .padding(theme.space2, 0.f, 0.f, 0.f)
                );
            }

            Element bubble = VStack {
                .spacing = theme.space4,
                .alignment = Alignment::Stretch,
                .children = std::move(paragraphs),
            }
                                 .padding(theme.space4)
                                 .fill(FillStyle::solid(fill))
                                 .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                                 .cornerRadius(theme.radiusXLarge);

            return HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    std::move(bubble).flex(0.f, 1.f, 0.f),
                    Spacer {}
                ),
            };
        }

        Element bubble = [&]() -> Element {
            if (isReasoning && collapsed) {
                Color const collapsedFill = pressed ? theme.colorSurfaceRowHover :
                                            hovered ? theme.colorSurfaceHover :
                                                      theme.colorSurface;
                std::vector<Element> collapsedChildren;
                collapsedChildren.push_back(
                    reasoningFinished ? Element {
                                           Text {
                                               .text = thoughtSummary,
                                               .font = theme.fontBodySmall,
                                               .color = theme.colorTextMuted,
                                               .horizontalAlignment = HorizontalAlignment::Leading,
                                               .wrapping = TextWrapping::Wrap,
                                           }
                                       } :
                                       Element {ThinkingDots {}}
                );
                if (message.generationStats.has_value() || hasDeleteAction) {
                    std::string primaryLine;
                    if (message.generationStats.has_value()) {
                        primaryLine =
                            joinNonEmpty(generationStatsPrimaryParts(*message.generationStats), "  •  ");
                    }
                    collapsedChildren.push_back(
                        Element { Divider { .orientation = Divider::Orientation::Horizontal } }
                    );
                    collapsedChildren.push_back(buildSummaryRow(std::move(primaryLine)));
                }

                return VStack {
                    .spacing = theme.space1,
                    .alignment = Alignment::Start,
                    .children = std::move(collapsedChildren),
                }
                    .padding(theme.space4)
                    .fill(FillStyle::solid(collapsedFill))
                    .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                    .cornerRadius(theme.radiusXLarge)
                    .cursor(Cursor::Hand)
                    .focusable(true)
                    .onTap(onToggleReasoning ? onToggleReasoning : std::function<void()> {});
            }

            std::vector<Element> contentChildren;
            contentChildren.push_back(
                Text {
                    .text = message.text,
                    .font = markdownStyle.baseFont,
                    .color = textColor,
                    .horizontalAlignment = HorizontalAlignment::Leading,
                    .verticalAlignment = VerticalAlignment::Top,
                    .wrapping = TextWrapping::Wrap,
                }
            );
            if (showSummaryFooter) {
                std::string primaryLine;
                if (message.generationStats.has_value()) {
                    primaryLine =
                        joinNonEmpty(generationStatsPrimaryParts(*message.generationStats), "  •  ");
                }
                std::vector<Element> footerChildren;
                footerChildren.push_back(
                    Divider { .orientation = Divider::Orientation::Horizontal }
                );
                footerChildren.push_back(buildSummaryRow(std::move(primaryLine)));
                contentChildren.push_back(
                    VStack {
                        .spacing = theme.space1,
                        .alignment = Alignment::Start,
                        .children = std::move(footerChildren),
                    }
                        .padding(theme.space1, 0.f, 0.f, 0.f)
                );
            }

            return VStack {
                .spacing = theme.space1,
                .alignment = Alignment::Start,
                .children = std::move(contentChildren)
            }
                .padding(theme.space4)
                .fill(FillStyle::solid(fill))
                .stroke(isUser ? StrokeStyle::none() : StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                .cornerRadius(theme.radiusXLarge)
                .cursor(isReasoning ? Cursor::Hand : Cursor::Arrow)
                .focusable(isReasoning)
                .onTap(isReasoning ? (onToggleReasoning ? onToggleReasoning : std::function<void()> {}) : std::function<void()> {});
        }();

        if (isUser) {
            return HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    Spacer {},
                    std::move(bubble).flex(0.f, 1.f, 0.f)
                ),
            };
        }

        return HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Start,
            .children = children(
                std::move(bubble).flex(0.f, 1.f, 0.f),
                Spacer {}
            ),
        };
    }
};

struct ChatComposer : ViewModifiers<ChatComposer> {
    State<std::string> value;
    bool disabled = true;
    std::string modelLabel;
    std::vector<LocalModel> localModels;
    std::string selectedModelPath;
    std::function<void(std::string const &)> onSend;
    std::function<void()> onStop;
    std::function<void(std::string const &, std::string const &)> onSelectModel;
    bool streaming = false;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto requestComposerFocus = useRequestFocus();
        auto clearComposerFocus = useClearFocus();
        bool const isDisabled = disabled;
        auto wasDisabled = useState(disabled);
        int derivedSelectedIndex = -1;
        std::vector<SelectOption> modelOptions;
        modelOptions.reserve(localModels.size());
        for (std::size_t i = 0; i < localModels.size(); ++i) {
            LocalModel const &model = localModels[i];
            PresentedLocalModel const presented = presentLocalModel(model);
            if (!selectedModelPath.empty() && model.path == selectedModelPath) {
                derivedSelectedIndex = static_cast<int>(i);
            }
            modelOptions.push_back(SelectOption {
                .label = presented.title.empty() ? model.name : presented.title,
                .detail = presented.detail,
            });
        }
        auto modelSelection = useState<int>(derivedSelectedIndex);

        if (*wasDisabled && !isDisabled) {
            Application::instance().onNextFrameNeeded([requestComposerFocus] {
                requestComposerFocus();
            });
        }
        wasDisabled = isDisabled;
        if (*modelSelection != derivedSelectedIndex) {
            modelSelection = derivedSelectedIndex;
        }

        auto draftState = value;
        bool const canSend = !disabled && !(*draftState).empty();
        auto submit = [draftState, onSend = onSend, canSend, clearComposerFocus]() {
            std::string message = *draftState;
            if (!canSend || message.empty() || !onSend) {
                return;
            }
            clearComposerFocus();
            onSend(message);
            draftState = "";
        };

        return VStack {
            .spacing = theme.space3,
            .alignment = Alignment::Start,
            .children = children(
                TextInput {
                    .value = value,
                    .placeholder = "Message Lambda Studio...",
                    .style = TextInput::Style::plain(),
                    .multiline = true,
                    .disabled = disabled,
                    .multilineHeight = {.minIntrinsic = 88.f, .maxIntrinsic = 160.f},
                    .onSubmit = [submit](std::string const &) {
                        submit();
                    },
                }
                    .onTap([requestComposerFocus, isDisabled] {
                        if (!isDisabled) {
                            requestComposerFocus();
                        }
                    }),
                HStack {
                    .spacing = theme.space2,
                    .alignment = Alignment::Center,
                    .children = children(Select {
                                             .selectedIndex = modelSelection,
                                             .options = std::move(modelOptions),
                                             .placeholder = modelLabel.empty() ? "Select model" : modelLabel,
                                             .emptyText = "No local models available",
                                             .disabled = localModels.empty() || streaming,
                                             .showDetailInTrigger = false,
                                             .matchTriggerWidth = false,
                                             .triggerMode = SelectTriggerMode::Link,
                                             .style = Select::Style {
                                                 .labelFont = theme.fontLabel,
                                                 .detailFont = theme.fontBodySmall,
                                                 .menuMaxHeight = 280.f,
                                                 .menuMaxWidth = 420.f,
                                                 .minMenuWidth = 0.f,
                                                 .accentColor = localModels.empty() || streaming ? theme.colorTextDisabled : theme.colorAccent,
                                             },
                                             .onChange = [localModels = localModels, onSelectModel = onSelectModel](int index) {
                                                 if (index < 0 || static_cast<std::size_t>(index) >= localModels.size()) {
                                                     return;
                                                 }
                                                 if (onSelectModel) {
                                                     LocalModel const &model = localModels[static_cast<std::size_t>(index)];
                                                     PresentedLocalModel const presented = presentLocalModel(model);
                                                     onSelectModel(model.path, presented.title.empty() ? model.name : presented.title);
                                                 }
                                             },
                                         },
                                         Spacer {}, streaming ? Element {IconButton {
                                                                    .icon = IconName::Cancel,
                                                                    .style = {
                                                                        .size = theme.fontHeading.size,
                                                                        .weight = theme.fontLabel.weight,
                                                                        .color = theme.colorTextSecondary,
                                                                    },
                                                                    .onTap = onStop,
                                                                }} :
                                                                Element {IconButton {
                                                                    .icon = IconName::ArrowUpward,
                                                                    .disabled = !canSend,
                                                                    .style = {
                                                                        .size = theme.fontHeading.size,
                                                                        .weight = theme.fontLabel.weight,
                                                                        .color = theme.colorAccent,
                                                                    },
                                                                    .onTap = submit,
                                                                }}),
                }
            ),
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(isDisabled ? theme.colorSurfaceDisabled : theme.colorSurfaceOverlay))
            .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
            .cornerRadius(theme.radiusXLarge)
            .onTap([requestComposerFocus, isDisabled] {
                if (!isDisabled) {
                    requestComposerFocus();
                }
            });
    }
};

struct ChatView : ViewModifiers<ChatView> {
    ChatThread chat;
    std::vector<LocalModel> localModels;
    std::string loadedModelPath;
    bool modelLoading = false;
    std::function<void(std::string const &)> onSend;
    std::function<void()> onStop;
    std::function<void()> onDeleteChat;
    std::function<void(int)> onToggleReasoning;
    std::function<void(int)> onDeleteMessage;
    std::function<void(std::string const &, std::string const &)> onSelectModel;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto draft = useState<std::string>("");

        std::string selectedModelLabel = "Select model";
        for (LocalModel const &model : localModels) {
            if (!chat.modelPath.empty() && model.path == chat.modelPath) {
                PresentedLocalModel const presented = presentLocalModel(model);
                selectedModelLabel = presented.title.empty() ? model.name : presented.title;
                break;
            }
        }

        bool const fakeStreaming = lambda_studio_backend::debugFakeStreamEnabled();
        bool const hasModel = fakeStreaming || !chat.modelPath.empty();
        bool const selectedModelReady = fakeStreaming || (hasModel && chat.modelPath == loadedModelPath);
        bool const canCompose = hasModel && selectedModelReady && (fakeStreaming || !modelLoading) && !chat.streaming;

        std::vector<Element> bubbles;
        bubbles.reserve(chat.messages.size() + chat.streamDraftMessages.size());
        for (std::size_t i = 0; i < chat.messages.size(); ++i) {
            ChatMessage const &message = chat.messages[i];
            bubbles.push_back(ChatBubble {
                .message = message,
                .deferTailMarkdown = false,
                .onToggleReasoning = [onToggleReasoning = onToggleReasoning, i] {
                    if (onToggleReasoning) {
                        onToggleReasoning(static_cast<int>(i));
                    }
                },
                .onDeleteMessage = [onDeleteMessage = onDeleteMessage, i] {
                    if (onDeleteMessage) {
                        onDeleteMessage(static_cast<int>(i));
                    }
                },
            });
        }
        std::size_t const committedCount = chat.messages.size();
        for (std::size_t i = 0; i < chat.streamDraftMessages.size(); ++i) {
            ChatMessage const &message = chat.streamDraftMessages[i];
            bool const isStreamingTail =
                chat.streaming && message.role == ChatRole::Assistant && i + 1 == chat.streamDraftMessages.size();
            bubbles.push_back(ChatBubble {
                .message = message,
                .deferTailMarkdown = isStreamingTail,
                .onToggleReasoning = [onToggleReasoning = onToggleReasoning, committedCount, i] {
                    if (onToggleReasoning) {
                        onToggleReasoning(static_cast<int>(committedCount + i));
                    }
                },
                .onDeleteMessage = [onDeleteMessage = onDeleteMessage, committedCount, i] {
                    if (onDeleteMessage) {
                        onDeleteMessage(static_cast<int>(committedCount + i));
                    }
                },
            });
        }

        return VStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                HStack {
                    .spacing = theme.space2,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = chat.title,
                            .font = theme.fontHeading,
                            .color = theme.colorTextPrimary,
                            .verticalAlignment = VerticalAlignment::Center
                        }
                            .flex(1.f, 1.f),
                        IconButton {
                            .icon = IconName::Delete,
                            .style = {
                                .size = theme.fontHeading.size,
                                .weight = theme.fontLabel.weight,
                                .color = theme.colorDanger,
                            },
                            .onTap = onDeleteChat,
                        }
                    ),
                }.padding(theme.space4)
                .fill(FillStyle::solid(theme.colorSurfaceOverlay)),
                Divider { .orientation = Divider::Orientation::Horizontal },
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = theme.space3,
                            .alignment = Alignment::Stretch,
                            .children = std::move(bubbles),
                        }
                            .padding(theme.space4)
                    )
                }
                    .flex(1.f, 1.f, 0.f)
                    .fill(FillStyle::solid(theme.colorBackground))
                    .clipContent(true),
                ChatComposer {
                    .value = draft,
                    .disabled = !canCompose,
                    .modelLabel = selectedModelLabel,
                    .localModels = localModels,
                    .selectedModelPath = chat.modelPath,
                    .onSend = onSend,
                    .onStop = onStop,
                    .onSelectModel = onSelectModel,
                    .streaming = chat.streaming,
                }.padding(0.f, theme.space4, theme.space4, theme.space4)
            ),
        }
            .fill(FillStyle::solid(theme.colorBackground))
            .clipContent(true)
            .flex(1.f, 1.f, 0.f);
    }
};

} // namespace lambda
