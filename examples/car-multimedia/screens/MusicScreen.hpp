#pragma once

#include "../Common.hpp"
#include "../AppState.hpp"
#include "../Helpers.hpp"
#include "../components/AccentIconButton.hpp"
#include "../components/AlbumArt.hpp"
#include "../components/ProgressLine.hpp"
#include "../components/Tab.hpp"
#include "QueueRow.hpp"

namespace car {

struct MusicScreen : ViewModifiers<MusicScreen> {
    Reactive::Signal<State> state;
    std::function<void(std::string)> onAction;
    auto body() const {
        auto theme = useEnvironment<ThemeKey>();
        Element playerCard = makeCard(
            HStack {
                .spacing = theme().space4,
                .alignment = Alignment::Stretch,
                .children = children(
                    AlbumArt {.seed = Reactive::Bindable<std::string> {[s = state] { return s().music.title; }}, .sizeP = 220.f},
                    VStack {
                        .spacing = theme().space1,
                        .alignment = Alignment::Start,
                        .children = children(
                            Text {.text = Reactive::Bindable<std::string> {[s = state] { return s().music.source; }}, .font = Font::caption2(), .color = Color::tertiary()},
                            Text {.text = Reactive::Bindable<std::string> {[s = state] { return s().music.title; }}, .font = Font {.size = 28.f, .weight = 500.f}, .color = Color::primary(), .wrapping = TextWrapping::NoWrap, .maxLines = 1},
                            Text {.text = Reactive::Bindable<std::string> {[s = state] { return s().music.artist; }}, .font = Font::body(), .color = Color::secondary()},
                            Text {.text = Reactive::Bindable<std::string> {[s = state] { return s().music.album; }}, .font = Font::subheadline(), .color = Color::tertiary()},
                            Spacer {}.flex(1.f, 1.f),
                            ProgressLine {.progress = Reactive::Bindable<float> {[s = state] { return s().music.progress; }}},
                            HStack {
                                .spacing = theme().space3,
                                .alignment = Alignment::Center,
                                .children = children(
                                    IconButton {.icon = IconName::Shuffle, .style = {.size = 36.f}},
                                    IconButton {.icon = IconName::SkipPrevious, .style = {.size = 36.f}, .onTap = [onAction = onAction] { if (onAction) onAction("prev"); }},
                                    AccentIconButton {.icon = Reactive::Bindable<IconName> {[s = state] { return s().music.playing ? IconName::Pause : IconName::PlayArrow; }}, .sizeP = 56.f, .onTap = [onAction = onAction] { if (onAction) onAction("toggle"); }},
                                    IconButton {.icon = IconName::SkipNext, .style = {.size = 36.f}, .onTap = [onAction = onAction] { if (onAction) onAction("next"); }},
                                    IconButton {.icon = IconName::Repeat, .style = {.size = 36.f}},
                                    Spacer {}.flex(1.f, 1.f),
                                    IconButton {.icon = IconName::Favorite, .style = {.size = 36.f}}
                                ),
                            }
                                .padding(18.f, 0.f, 0.f, 0.f)
                        ),
                    }
                        .flex(1.f, 1.f, 0.f)
                ),
            },
            theme().space4
        );
        Element queueHeader = HStack {
            .spacing = theme().space3,
            .alignment = Alignment::Center,
            .children = children(
                Icon{.name = IconName::QueueMusic, .size = 18.f, .color = Color::tertiary()},
                Text{.text = "Up Next", .font = Font{.size = 13.f, .weight = 500.f}, .color = Color::primary()},
                Spacer{}.flex(1.f, 1.f),
                Tab{.label = "Queue", .active = true},
                Tab{.label = "Library"},
                Tab{.label = "Radio"}
            ),
        }.padding(theme().space3, theme().space5, theme().space3, theme().space5);
        std::vector<Element> rows;
        std::vector<QueueItem> queue = state().queue;
        for (std::size_t i = 0; i < queue.size(); ++i) {
            rows.push_back(QueueRow{.index = static_cast<int>(i), .title = queue[i].title, .artist = queue[i].artist, .duration = queue[i].duration, .active = i == 0});
            rows.push_back(Divider{.orientation = Divider::Orientation::Horizontal});
        }
        Element queueCard = Card {
            .child = VStack {
                .spacing = 0.f,
                .alignment = Alignment::Stretch,
                .children = children(
                    queueHeader,
                    Divider{.orientation = Divider::Orientation::Horizontal},
                    ScrollView{.axis = ScrollAxis::Vertical, .children = children(VStack{.spacing = 0.f, .alignment = Alignment::Stretch, .children = std::move(rows)})}.flex(1.f, 1.f, 0.f)
                ),
            },
            .style = Card::Style{.padding = 0.f, .cornerRadius = 10.f, .backgroundColor = Color::elevatedBackground(), .borderColor = Color::separator()},
        };
        return Grid {
            .columns = 5,
            .horizontalSpacing = theme().space4,
            .verticalSpacing = theme().space4,
            .horizontalAlignment = Alignment::Stretch,
            .verticalAlignment = Alignment::Stretch,
            .children = children(
                std::move(playerCard).colSpan(3u),
                std::move(queueCard).colSpan(2u)
            ),
        }
            .padding(theme().space4);
    }
};

} // namespace car
