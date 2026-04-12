#pragma once

#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "AppState.hpp"
#include "ChatModels.hpp"

using namespace flux;

namespace lambda {

struct IconActionButton : ViewModifiers<IconActionButton> {
    IconName icon = IconName::Add;
    bool disabled = false;
    std::function<void()> onTap;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto const hovered = useHover();
        auto const pressed = usePress();

        auto const getFillColor = [theme, disabled = disabled, hovered, pressed]() {
            if (disabled) {
                return theme.colorSurfaceDisabled;
            }

            if (hovered) {
                return theme.colorSurfaceHover;
            }

            if (pressed) {
                return theme.colorSurfaceRowHover;
            }

            return theme.colorSurface;
        };

        auto const getIconColor = [theme, disabled = disabled, hovered, pressed]() {
            if (disabled) {
                return theme.colorTextDisabled;
            }

            if (hovered || pressed) {
                return theme.colorAccentSubtle;
            }

            return theme.colorAccent;
        };

        auto const fillColor = getFillColor();
        auto const iconColor = getIconColor();

        return Icon {
            .name = icon,
            .size = 18.f,
            .weight = 700.f,
            .color = iconColor,
        }
            .fill(FillStyle::solid(fillColor))
            .stroke(StrokeStyle::solid(iconColor, 1.f))
            .size(32.f, 32.f)
            .cornerRadius(theme.radiusFull)
            .cursor(disabled ? Cursor::Arrow : Cursor::Hand)
            .onTap([disabled = disabled, onTap = onTap] {
                if (!disabled && onTap) {
                    onTap();
                }
            });
    }
};

struct ChatBubble : ViewModifiers<ChatBubble> {
    ChatMessage message;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        bool const isUser = message.role == ChatRole::User;
        bool const isReasoning = message.role == ChatRole::Reasoning;
        Color const fill = isUser      ? theme.colorAccent :
                           isReasoning ? theme.colorBackground :
                                         theme.colorSurfaceOverlay;
        Color const textColor = isUser ? theme.colorTextOnAccent : theme.colorTextPrimary;
        std::string const label = isUser ? "You" : isReasoning ? "Reasoning" :
                                                                 "Lambda";

        Element bubble = VStack {
            .spacing = theme.space1,
            .alignment = Alignment::Start,
            .children = children(
                Text {
                    .text = label,
                    .font = theme.fontLabelSmall,
                    .color = isUser      ? theme.colorTextOnAccent :
                             isReasoning ? theme.colorTextMuted :
                                           theme.colorTextSecondary,
                    .horizontalAlignment = HorizontalAlignment::Leading,
                },
                Text {
                    .text = message.text,
                    .font = isReasoning ? theme.fontBodySmall : theme.fontBody,
                    .color = textColor,
                    .horizontalAlignment = HorizontalAlignment::Leading,
                    .verticalAlignment = VerticalAlignment::Top,
                    .wrapping = TextWrapping::Wrap,
                }
            ),
        }
                             .padding(theme.space4)
                             .fill(FillStyle::solid(fill))
                             .stroke(isUser ? StrokeStyle::none() : StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
                             .cornerRadius(theme.radiusXLarge);

        if (isUser) {
            return HStack {
                .spacing = theme.space3,
                .alignment = Alignment::Start,
                .children = children(
                    Spacer {},
                    std::move(bubble)
                ),
            };
        }

        return HStack {
            .spacing = theme.space3,
            .alignment = Alignment::Start,
            .children = children(
                std::move(bubble),
                Spacer {}
            ),
        };
    }
};

struct ChatComposer : ViewModifiers<ChatComposer> {
    State<std::string> value;
    bool disabled = true;
    std::string hintText;
    std::function<void(std::string const &)> onSend;
    std::function<void()> onStop;
    bool streaming = false;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

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
                },
                HStack {
                    .spacing = theme.space3,
                    .alignment = Alignment::Center,
                    .children = children(Icon {
                                             .name = IconName::Add,
                                             .size = 20.f,
                                             .weight = 500.f,
                                             .color = theme.colorTextSecondary,
                                         }
                                             .cursor(Cursor::Hand),
                                         Text {
                                             .text = hintText.empty() ? "Local draft" : hintText,
                                             .font = theme.fontBodySmall,
                                             .color = disabled ? theme.colorTextMuted : theme.colorTextSecondary,
                                             .verticalAlignment = VerticalAlignment::Center,
                                         },
                                         Spacer {}, streaming ? IconActionButton {.icon = IconName::Stop, .onTap = onStop} : IconActionButton {
                                                                                                                                 .icon = IconName::Send,
                                                                                                                                 .disabled = !canSend,
                                                                                                                                 .onTap = submit,
                                                                                                                             }),
                }
            ),
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(disabled ? theme.colorSurfaceDisabled : theme.colorSurfaceOverlay))
            .stroke(StrokeStyle::solid(theme.colorBorderSubtle, 1.f))
            .cornerRadius(theme.radiusXLarge);
    }
};

struct ChatView : ViewModifiers<ChatView> {
    ChatThread chat;
    std::vector<LocalModel> localModels;
    std::string loadedModelPath;
    bool modelLoading = false;
    std::function<void(std::string const &)> onSend;
    std::function<void()> onStop;
    std::function<void(std::string const &, std::string const &)> onSelectModel;

    auto body() const {
        Theme const &theme = useEnvironment<Theme>();
        auto draft = useState<std::string>("");
        std::vector<SelectOption> modelOptions;
        modelOptions.reserve(localModels.size());

        int currentModelIndex = -1;
        for (std::size_t i = 0; i < localModels.size(); ++i) {
            LocalModel const &model = localModels[i];
            std::string detail = formatModelSize(model.sizeBytes);
            if (!model.path.empty()) {
                detail = detail.empty() ? model.path : detail + "  " + model.path;
            }
            modelOptions.push_back(SelectOption {
                .label = model.name,
                .detail = detail,
            });
            if (!chat.modelPath.empty() && model.path == chat.modelPath) {
                currentModelIndex = static_cast<int>(i);
            }
        }

        auto selectedModelIndex = useState<int>(currentModelIndex);
        if (*selectedModelIndex != currentModelIndex) {
            selectedModelIndex = currentModelIndex;
        }

        bool const hasModel = !chat.modelPath.empty();
        bool const selectedModelReady = hasModel && chat.modelPath == loadedModelPath;
        bool const canCompose = hasModel && selectedModelReady && !modelLoading && !chat.streaming;
        std::string composerHint = "Select a model to start chatting.";
        if (modelLoading && chat.modelPath == loadedModelPath) {
            composerHint = "Model is loading...";
        } else if (modelLoading && !chat.modelPath.empty() && chat.modelPath != loadedModelPath) {
            composerHint = "Loading the selected model...";
        } else if (chat.streaming) {
            composerHint = "Streaming response...";
        } else if (selectedModelReady) {
            composerHint = "Ready";
        } else if (hasModel) {
            composerHint = "Load the selected model to send.";
        }

        std::vector<Element> bubbles;
        bubbles.reserve(chat.messages.size());
        for (ChatMessage const &message : chat.messages) {
            bubbles.push_back(ChatBubble {.message = message});
        }

        return VStack {
            .spacing = theme.space4,
            .alignment = Alignment::Center,
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
                                },
                                Text {
                                    .text = chat.modelName.empty() ? "No model selected" :
                                                                     "Model: " + chat.modelName + "  •  Updated " + chat.updatedAt,
                                    .font = theme.fontLabelSmall,
                                    .color = theme.colorTextSecondary,
                                    .verticalAlignment = VerticalAlignment::Center
                                }
                            )
                        }
                            .flex(1.f, 1.f),
                        Select {
                            .selectedIndex = selectedModelIndex,
                            .options = std::move(modelOptions),
                            .placeholder = "Select model",
                            .disabled = localModels.empty() || chat.streaming,
                            .style = Select::Style {
                                .minMenuWidth = 320.f,
                            },
                            .onChange = [onSelectModel = onSelectModel, localModels = localModels](int index) {
                                if (index < 0 || static_cast<std::size_t>(index) >= localModels.size() || !onSelectModel) {
                                    return;
                                }
                                LocalModel const &model = localModels[static_cast<std::size_t>(index)];
                                onSelectModel(model.path, model.name);
                            },
                        }
                            .size(300.f, 0.f)
                    ),
                },
                ScrollView {
                    .axis = ScrollAxis::Vertical,
                    .children = children(VStack {
                        .spacing = theme.space3,
                        .children = std::move(bubbles),
                    }),
                }
                    .flex(1.f, 1.f, 0.f),
                ChatComposer {
                    .value = draft,
                    .disabled = !canCompose,
                    .hintText = composerHint,
                    .onSend = onSend,
                    .onStop = onStop,
                    .streaming = chat.streaming,
                }
            ),
        }
            .padding(theme.space4)
            .fill(FillStyle::solid(theme.colorBackground))
            .flex(1.f, 1.f, 0.f);
    }
};

} // namespace lambda
