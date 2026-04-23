#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <string>
#include <tuple>

using namespace flux;

const std::vector<std::tuple<std::string, Cursor>> cursors = {
    {"Default", Cursor::Arrow},
    {"Hand", Cursor::Hand},
    {"ResizeEW", Cursor::ResizeEW},
    {"ResizeNS", Cursor::ResizeNS},
    {"ResizeNESW", Cursor::ResizeNESW},
    {"ResizeNWSE", Cursor::ResizeNWSE},
    {"ResizeAll", Cursor::ResizeAll},
    {"Crosshair", Cursor::Crosshair},
    {"NotAllowed", Cursor::NotAllowed},
};

/// Hover each row to see the platform cursor; drag the bottom strip to verify the cursor stays locked.
struct CursorDemo {
    auto body() const {
        Theme const &theme = useEnvironment<Theme>();

        std::vector<Element> cells = {};
        for (auto const &[name, cursor] : cursors) {
            cells.push_back(
                Text {
                    .text = name,
                    .horizontalAlignment = HorizontalAlignment::Center,
                    .verticalAlignment = VerticalAlignment::Center,
                }
                    // .cornerRadius(theme.radiusXLarge)
                    .cursor(cursor)
                    .fill(Color::elevatedBackground())
                    .height(200.f)
            );
        }

        return ScrollView {
            .axis = ScrollAxis::Vertical,
            .children = children(
                VStack {
                    .spacing = theme.space4,
                    .children = children(
                        Text {
                            .text = "Cursor Demo",
                            .font = Font::largeTitle(),
                            .color = Color::primary(),
                        },
                        Text {
                            .text = "Move the pointer over each swatch. Drag the resize strip — the cursor stays locked to that node during the drag.",
                            .font = Font::body(),
                            .color = Color::secondary(),
                            .wrapping = TextWrapping::Wrap,
                        },
                        Grid {
                            .columns = 3,
                            .horizontalSpacing = theme.space1,
                            .verticalSpacing = theme.space1,
                            .horizontalAlignment = Alignment::Stretch,
                            .verticalAlignment = Alignment::Stretch,
                            .children = std::move(cells)
                        }
                    )
                }
            )
        }
            .padding(theme.space5);
    }
};

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    auto &w = app.createWindow<Window>({
        .size = {800, 800},
        .title = "Flux — Cursor demo",
        .resizable = true,
    });
    w.setView<CursorDemo>();
    return app.exec();
}
