#pragma once

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Reactive/Transition.hpp>
#include <Flux/UI/StateStore.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "AppState.hpp"
#include "ChatModels.hpp"

using namespace flux;

namespace lambda {

namespace {

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

struct ModelMenuRow : ViewModifiers<ModelMenuRow> {
    LocalModel model;
    bool selected = false;
    bool disabled = false;
    std::function<void()> onTap;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        bool const hovered = useHover();
        bool const pressed = usePress();
        bool const isDisabled = disabled;

        Color const fill = selected ? theme.colorAccentSubtle : pressed ? theme.colorSurfaceRowHover :
                                                            hovered     ? theme.colorSurfaceHover :
                                                                          Colors::transparent;

        std::string const sizeLabel = formatModelSize(model.sizeBytes);

        return HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Center,
            .children = children(
                VStack {
                    .spacing = theme.space1 * 0.5f,
                    .alignment = Alignment::Stretch,
                    .children = children(
                        Text {
                            .text = model.name,
                            .font = theme.fontLabel,
                            .color = disabled ? theme.colorTextDisabled : theme.colorTextPrimary,
                            .horizontalAlignment = HorizontalAlignment::Leading,
                            .wrapping = TextWrapping::Wrap,
                        },
                        HStack {
                            .spacing = theme.space2,
                            .alignment = Alignment::Center,
                            .children = children(
                                Text {
                                    .text = model.path,
                                    .font = theme.fontBodySmall,
                                    .color = disabled ? theme.colorTextDisabled : theme.colorTextSecondary,
                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                    .wrapping = TextWrapping::NoWrap,
                                    .maxLines = 1,
                                }
                                    .flex(1.f, 1.f, 0.f),
                                Text {
                                    .text = sizeLabel,
                                    .font = theme.fontBodySmall,
                                    .color = disabled ? theme.colorTextDisabled : theme.colorTextSecondary,
                                    .horizontalAlignment = HorizontalAlignment::Trailing,
                                    .wrapping = TextWrapping::NoWrap,
                                    .maxLines = 1,
                                }
                            )
                        }
                    ),
                }
                    .flex(1.f, 1.f),
                selected ? Element {Icon {
                               .name = IconName::Check,
                               .size = 16.f,
                               .color = theme.colorAccent,
                           }} :
                           Element {Spacer {}.size(16.f, 16.f)}
            ),
        }
            .padding(theme.space3, theme.space4, theme.space3, theme.space4)
            .fill(FillStyle::solid(fill))
            .cursor(isDisabled ? Cursor::Arrow : Cursor::Hand)
            .focusable(!isDisabled)
            .onTap([onTap = onTap, isDisabled] {
                if (!isDisabled && onTap) {
                    onTap();
                }
            });
    }
};

struct ModelPickerButton : ViewModifiers<ModelPickerButton> {
    std::string label;
    std::vector<LocalModel> models;
    std::string selectedModelPath;
    bool disabled = false;
    std::function<void(std::string const &, std::string const &)> onSelect;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto [showPopover, hidePopover, menuOpen] = usePopover();
        (void)menuOpen;
        bool const isDisabled = disabled;

        std::vector<Element> rows;
        rows.reserve(models.size());

        for (std::size_t i = 0; i < models.size(); ++i) {
            LocalModel const &model = models[i];
            rows.push_back(ModelMenuRow {
                .model = model,
                .selected = !selectedModelPath.empty() && model.path == selectedModelPath,
                .disabled = disabled,
                .onTap = [onSelect = onSelect, hidePopover, path = model.path, name = model.name] {
                    if (onSelect) {
                        onSelect(path, name);
                    }
                    hidePopover();
                },
            });
        }

        Element emptyMenu = Text {
            .text = "No local models available",
            .font = theme.fontBodySmall,
            .color = theme.colorTextSecondary,
            .horizontalAlignment = HorizontalAlignment::Leading,
            .wrapping = TextWrapping::Wrap,
        }
                                .padding(theme.space3, theme.space4, theme.space3, theme.space4)
                                .width(280.f);

        auto openMenu = [showPopover, theme, rows = std::move(rows), emptyMenu = std::move(emptyMenu)]() mutable {
            Element menuContent = std::move(emptyMenu);
            if (!rows.empty()) {
                menuContent = ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = 0.f,
                            .alignment = Alignment::Stretch,
                            .children = std::move(rows),
                        }
                    )
                }
                                  .width(420.f)
                                  .height(280.f)
                                  .clipContent(true);
            } else {
                menuContent = Text {
                    .text = "No local models available",
                    .font = theme.fontBodySmall,
                    .color = theme.colorTextSecondary,
                    .horizontalAlignment = HorizontalAlignment::Leading,
                    .wrapping = TextWrapping::Wrap,
                }
                                  .padding(theme.space3, theme.space4, theme.space3, theme.space4)
                                  .width(280.f);
            }

            showPopover(Popover {
                .content = std::move(menuContent),
                .placement = PopoverPlacement::Below,
                .gap = theme.space1,
                .arrow = false,
                .cornerRadius = theme.radiusLarge,
                .contentPadding = 0.f,
                .maxSize = Size {380.f, 280.f},
                .backdropColor = Colors::transparent,
                .dismissOnEscape = true,
                .dismissOnOutsideTap = true,
                .useTapAnchor = false,
                .debugName = "lambda.model_picker",
            });
        };

        return LinkButton {
            .label = label,
            .disabled = disabled,
            .style = LinkButton::Style {
                .font = theme.fontLabel,
                .color = isDisabled ? theme.colorTextDisabled : theme.colorAccent,
            },
            .onTap = [openMenu, isDisabled]() mutable {
                if (!isDisabled) {
                    openMenu();
                }
            },
        }
            .cursor(isDisabled ? Cursor::Arrow : Cursor::Hand);
    }
};

struct ChatBubble : ViewModifiers<ChatBubble> {
    ChatMessage message;
    std::function<void()> onToggleReasoning;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        bool const isUser = message.role == ChatRole::User;
        bool const isReasoning = message.role == ChatRole::Reasoning;
        bool const hovered = useHover();
        bool const pressed = usePress();
        bool const collapsed = isReasoning && message.collapsed;
        bool const reasoningFinished = message.finishedAtNanos > message.startedAtNanos;
        std::string const thoughtSummary = formatThoughtDuration(message.startedAtNanos, message.finishedAtNanos);

        Color const fill = isUser      ? theme.colorAccent :
                           isReasoning ? theme.colorSurface :
                                         theme.colorSurfaceOverlay;
        Color const textColor = isUser ? theme.colorTextOnAccent : theme.colorTextPrimary;

        Element bubble = [&]() -> Element {
            if (isReasoning && collapsed) {
                Color const collapsedFill = pressed ? theme.colorSurfaceRowHover :
                                            hovered ? theme.colorSurfaceHover :
                                                      theme.colorSurface;
                return (reasoningFinished ? Element {
                                                Text {
                                                    .text = thoughtSummary,
                                                    .font = theme.fontBodySmall,
                                                    .color = theme.colorTextMuted,
                                                    .horizontalAlignment = HorizontalAlignment::Leading,
                                                    .wrapping = TextWrapping::Wrap,
                                                }
                                            } :
                                            Element {ThinkingDots {}})
                    .padding(theme.space4)
                    .fill(FillStyle::solid(collapsedFill))
                    .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                    .cornerRadius(theme.radiusXLarge)
                    .cursor(Cursor::Hand)
                    .focusable(true)
                    .onTap(onToggleReasoning ? onToggleReasoning : std::function<void()> {});
            }

            return VStack {
                .spacing = theme.space1,
                .alignment = Alignment::Start,
                .children = children(
                    Text {
                        .text = message.text,
                        .font = isReasoning ? theme.fontBodySmall : theme.fontBody,
                        .color = textColor,
                        .horizontalAlignment = HorizontalAlignment::Leading,
                        .verticalAlignment = VerticalAlignment::Top,
                        .wrapping = TextWrapping::Wrap,
                    },
                    (isReasoning && reasoningFinished) ? Element {
                                                             Text {
                                                                 .text = thoughtSummary,
                                                                 .font = theme.fontBodySmall,
                                                                 .color = theme.colorTextMuted,
                                                                 .horizontalAlignment = HorizontalAlignment::Leading,
                                                                 .wrapping = TextWrapping::Wrap,
                                                             }
                                                         } :
                                                         Element {Spacer {}.size(0.f, 0.f)}
                ),
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
        bool const isDisabled = disabled;
        auto wasDisabled = useState(disabled);

        if (*wasDisabled && !isDisabled) {
            Application::instance().onNextFrameNeeded([requestComposerFocus] {
                requestComposerFocus();
            });
        }
        wasDisabled = isDisabled;

        auto draftState = value;
        bool const canSend = !disabled && !(*draftState).empty();
        auto submit = [draftState, onSend = onSend, canSend]() {
            std::string message = *draftState;
            if (!canSend || message.empty() || !onSend) {
                return;
            }
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
                    .children = children(ModelPickerButton {
                                             .label = modelLabel.empty() ? "Select model" : modelLabel,
                                             .models = localModels,
                                             .selectedModelPath = selectedModelPath,
                                             .disabled = localModels.empty() || streaming,
                                             .onSelect = onSelectModel,
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
    std::function<void(int)> onToggleReasoning;
    std::function<void(std::string const &, std::string const &)> onSelectModel;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto draft = useState<std::string>("");

        std::string selectedModelLabel = "Select model";
        for (LocalModel const &model : localModels) {
            if (!chat.modelPath.empty() && model.path == chat.modelPath) {
                selectedModelLabel = model.name;
                break;
            }
        }

        bool const hasModel = !chat.modelPath.empty();
        bool const selectedModelReady = hasModel && chat.modelPath == loadedModelPath;
        bool const canCompose = hasModel && selectedModelReady && !modelLoading && !chat.streaming;

        std::vector<Element> bubbles;
        bubbles.reserve(chat.messages.size());
        for (std::size_t i = 0; i < chat.messages.size(); ++i) {
            ChatMessage const &message = chat.messages[i];
            bubbles.push_back(ChatBubble {
                .message = message,
                .onToggleReasoning = [onToggleReasoning = onToggleReasoning, i] {
                    if (onToggleReasoning) {
                        onToggleReasoning(static_cast<int>(i));
                    }
                },
            });
        }

        return VStack {
            .spacing = theme.space4,
            .alignment = Alignment::Stretch,
            .children = children(
                HStack {
                    .spacing = theme.space2,
                    .alignment = Alignment::Center,
                    .children = children(
                        VStack {
                            .spacing = theme.space1,
                            .alignment = Alignment::Start,
                            .children = children(
                                Text {
                                    .text = chat.title,
                                    .font = theme.fontHeading,
                                    .color = theme.colorTextPrimary,
                                    .verticalAlignment = VerticalAlignment::Center
                                }
                            )
                        }
                            .flex(1.f, 1.f)
                    ),
                },
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(
                        VStack {
                            .spacing = theme.space3,
                            .alignment = Alignment::Stretch,
                            .children = std::move(bubbles),
                        }
                            .padding(0.f, 0.f, theme.space2, 0.f)
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
                }
            ),
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(theme.colorBackground))
            .clipContent(true)
            .flex(1.f, 1.f, 0.f);
    }
};

} // namespace lambda
