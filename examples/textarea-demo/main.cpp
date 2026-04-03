// Multiline TextArea: Escape clears notes (onEscape).
// Styling uses chained Element modifiers (background, border, corner radius, clip, flex) instead of
// duplicating chrome fields on the struct.

#include <Flux.hpp>
#include <Flux/Core/Action.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/Reactive/Reactive.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextArea.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <cstdio>
#include <string>

using namespace flux;

struct NotesEditor {
    auto body() const {
        auto theme = useEnvironment<Theme>();

        auto text = useState(std::string {});

        return VStack {
            .spacing = 10.f,
            .children = children(
                Text {
                    .text = "Notes",
                    .style = theme.typeDisplay,
                    .color = theme.colorTextPrimary,
                },
                Text {
                    .text = "Shell styling uses .background(), .border(), .cornerRadius(), .clipContent(), and .flex() on the TextArea.",
                    .style = theme.typeBody,
                    .color = theme.colorTextSecondary,
                    .wrapping = TextWrapping::Wrap
                },
                TextArea {
                    .value = text,
                    .placeholder = "Start typing…",
                    .height = {.minIntrinsic = 120.f, .maxIntrinsic = 400.f},
                    .onChange = [](std::string const& v) {
                        std::fprintf(stderr, "notes length=%zu\n", v.size());
                    },
                    .onEscape = [text](std::string const&) { text = ""; },
                }.flex(1.f, 1.f, 0.f)
            ),
        }.flex(1.f, 1.f, 0.f).padding(20.f);
    }
};

int main(int argc, char* argv[]) {
    Application app(argc, argv);
    auto& w = app.createWindow<Window>({
        .title = "Flux — TextArea"
    });

    w.registerAction("edit.copy", {.label = "Copy", .shortcut = shortcuts::Copy});
    w.registerAction("edit.cut", {.label = "Cut", .shortcut = shortcuts::Cut});
    w.registerAction("edit.paste", {.label = "Paste", .shortcut = shortcuts::Paste});
    w.registerAction("edit.selectAll", {.label = "Select All", .shortcut = shortcuts::SelectAll});
    w.registerAction("app.quit", {.label = "Quit", .shortcut = shortcuts::Quit, .isEnabled = [] { return true; }});

    w.setView<NotesEditor>();
    return app.exec();
}
