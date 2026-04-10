#pragma once

#include <Flux.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <cstdio>
#include <functional>
#include <string>

#include "Divider.hpp"
#include "Types.hpp"

using namespace flux;

struct ParamSlider {
    std::string label;
    std::string valueText;
    State<float> value {};
    float min = 0.f;
    float max = 1.f;
    float step = 0.01f;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();
        return VStack {
            .spacing = 4.f,
            .children = children(
                HStack {
                    .spacing = 4.f,
                    .alignment = Alignment::Center,
                    .children = children(
                        Text {
                            .text = label,
                            .font = theme.fontBodySmall,
                            .color = theme.colorTextPrimary,
                        },
                        Spacer {},
                        Text {
                            .text = valueText,
                            .font = theme.fontBodySmall,
                            .color = theme.colorTextSecondary,
                        }
                    )
                },
                Slider {
                    .value = value,
                    .min = min,
                    .max = max,
                    .step = step,
                }.flex(1.f)
            )
        };
    }
};

struct PropertiesPanel : ViewModifiers<PropertiesPanel> {
    std::string modelPath;
    std::string modelName;

    State<float> temperature {};
    State<float> topP {};
    State<float> topK {};
    State<float> maxTokens {};

    std::function<void(SamplingParams const&)> onParamsChanged;

    auto body() const {
        Theme const& theme = useEnvironment<Theme>();

        float tempVal = *temperature;
        float topPVal = *topP;
        float topKVal = *topK;
        float maxTokVal = *maxTokens;

        char tempBuf[16], topPBuf[16], topKBuf[16], maxTokBuf[16];
        std::snprintf(tempBuf, sizeof(tempBuf), "%.2f", tempVal);
        std::snprintf(topPBuf, sizeof(topPBuf), "%.2f", topPVal);
        std::snprintf(topKBuf, sizeof(topKBuf), "%d", static_cast<int>(topKVal));
        std::snprintf(maxTokBuf, sizeof(maxTokBuf), "%d", static_cast<int>(maxTokVal));

        std::string statusText = modelPath.empty() ? "No model loaded" : modelName;

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = 8.f,
                    .children = children(
                        Text {
                            .text = "Properties",
                            .font = theme.fontTitle,
                            .color = theme.colorTextPrimary,
                        }.padding(16.f, 8.f, 8.f, 8.f),
                        Divider {},
                        Text {
                            .text = statusText,
                            .font = theme.fontBody,
                            .color = theme.colorTextSecondary,
                            .wrapping = TextWrapping::Wrap,
                        }.padding(8.f, 16.f, 4.f, 16.f),
                        Text {
                            .text = modelPath.empty() ? std::string{} : modelPath,
                            .font = theme.fontBodySmall,
                            .color = theme.colorTextMuted,
                            .wrapping = TextWrapping::Wrap,
                        }.padding(0.f, 16.f, 8.f, 16.f),
                        Divider {},
                        Text {
                            .text = "Sampling",
                            .font = theme.fontLabel,
                            .color = theme.colorTextPrimary,
                        }.padding(8.f, 16.f, 4.f, 16.f),
                        Element { ParamSlider {
                            .label = "Temperature",
                            .valueText = tempBuf,
                            .value = temperature,
                            .min = 0.f,
                            .max = 2.f,
                            .step = 0.05f,
                        }}.padding(0.f, 16.f, 0.f, 16.f),
                        Element { ParamSlider {
                            .label = "Top-P",
                            .valueText = topPBuf,
                            .value = topP,
                            .min = 0.f,
                            .max = 1.f,
                            .step = 0.05f,
                        }}.padding(0.f, 16.f, 0.f, 16.f),
                        Element { ParamSlider {
                            .label = "Top-K",
                            .valueText = topKBuf,
                            .value = topK,
                            .min = 0.f,
                            .max = 100.f,
                            .step = 1.f,
                        }}.padding(0.f, 16.f, 0.f, 16.f),
                        Element { ParamSlider {
                            .label = "Max Tokens",
                            .valueText = maxTokBuf,
                            .value = maxTokens,
                            .min = 64.f,
                            .max = 8192.f,
                            .step = 64.f,
                        }}.padding(0.f, 16.f, 0.f, 16.f)
                    )
                }
            )
        }.size(280.f, 0.f);
    }
};