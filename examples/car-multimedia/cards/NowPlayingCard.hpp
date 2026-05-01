#pragma once

#include "../AppState.hpp"
#include "../Common.hpp"
#include "../Helpers.hpp"
#include "../components/AccentIconButton.hpp"
#include "../components/AlbumArt.hpp"
#include "../components/CardHeader.hpp"
#include "../components/ProgressLine.hpp"

namespace car {

struct NowPlayingCard : ViewModifiers<NowPlayingCard> {
    Reactive::Signal<State> state;
    std::function<void(std::string)> onAction;

    auto body() const {
        auto theme = useEnvironment<ThemeKey>();

        Element body = VStack {
            .spacing = 0.f,
            .alignment = Alignment::Stretch,
            .children = children(
                CardHeader {
                    .icon = IconName::LibraryMusic,
                    .title = "NOW PLAYING"
                },
                HStack {
                    .spacing = theme().space3,
                    .alignment = Alignment::Center,
                    .children = children(
                        AlbumArt {
                            .seed = [s = state] { return s().music.title; },
                            .sizeP = 64.f
                        },
                        VStack {
                            .spacing = theme().space1,
                            .alignment = Alignment::Start,
                            .children = children(
                                Text {
                                    .text = [s = state] { return s().music.title; },
                                    .font = Font {.size = 15.f, .weight = 600.f},
                                    .color = Color::primary(),
                                    .wrapping = TextWrapping::NoWrap,
                                    .maxLines = 1
                                },
                                Text {
                                    .text = [s = state] { return s().music.artist; },
                                    .font = Font::subheadline(),
                                    .color = Color::secondary(),
                                    .wrapping = TextWrapping::NoWrap,
                                    .maxLines = 1
                                },
                                Text {
                                    .text = [s = state] { return s().music.source; },
                                    .font = Font::caption2(),
                                    .color = Color::tertiary()
                                }
                            ),
                        }
                            .flex(1.f, 1.f, 0.f)
                    ),
                }
                    .padding(0.f, 0.f, 14.f, 0.f),
                Spacer {}.flex(1.f, 1.f),
                ProgressLine {
                    .progress = [s = state] { return s().music.progress; }
                },
                HStack {
                    .spacing = 0.f,
                    .alignment = Alignment::Center,
                    .justifyContent = JustifyContent::SpaceBetween,
                    .children = children(
                        IconButton {
                            .icon = IconName::SkipPrevious,
                            .style = IconButton::Style {.size = 18.f},
                            .onTap = [onAction = onAction] { if (onAction) onAction("prev"); }
                        },
                        AccentIconButton {
                            .icon = [s = state] { return s().music.playing ? IconName::Pause : IconName::PlayArrow; },
                            .sizeP = 44.f,
                            .onTap = [onAction = onAction] {
                                if (onAction) {
                                    onAction("toggle");
                                }
                            }
                        },
                        IconButton {
                            .icon = IconName::SkipNext,
                            .style = IconButton::Style {.size = 18.f},
                            .onTap = [onAction = onAction] {
                                if (onAction) {
                                    onAction("next");
                                }
                            }
                        }
                    ),
                }
                    .padding(14.f, 0.f, 0.f, 0.f)
            ),
        };

        return makeCard(body, 20.f);
    }
};

} // namespace car
