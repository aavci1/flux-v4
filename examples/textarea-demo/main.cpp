// Multiline TextArea: Escape clears notes (onEscape).
// Styling uses chained Element modifiers (fill, stroke, corner radius, clip, flex) instead of
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

        auto text = useState(std::string {
R"(Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec dictum mi et metus porta malesuada. Nullam vitae urna orci. Sed sit amet vulputate erat, vel molestie velit. Etiam mollis dui non vehicula sagittis. Sed mollis tellus eget magna blandit congue. Nulla varius ipsum at eros congue, posuere dignissim tortor tincidunt. Duis sollicitudin at felis non pretium. Orci varius natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Phasellus efficitur blandit lacus, non porta orci feugiat non. Maecenas pharetra ultrices ante ut tristique. Vestibulum mattis diam et ultrices faucibus. Mauris cursus orci vel arcu blandit bibendum. Interdum et malesuada fames ac ante ipsum primis in faucibus.

In facilisis odio sit amet pulvinar fringilla. Aliquam accumsan, urna dictum scelerisque lobortis, eros turpis volutpat sapien, et commodo nisl sapien sed ipsum. Vivamus molestie in felis eget pretium. Vestibulum eleifend ac leo sit amet mattis. Vivamus condimentum tincidunt faucibus. Etiam a auctor tellus. Morbi porta, magna sit amet fringilla suscipit, massa lorem vulputate nulla, sit amet convallis orci libero porta mi. Phasellus ut est eget purus lobortis molestie sed sit amet magna. Ut rutrum elementum nisi, sollicitudin tempor ex luctus id. Nunc consequat sapien non nisi viverra, in consectetur est cursus. In in sapien id tortor tristique porttitor sed a arcu. Nulla vestibulum elit vel commodo pretium. Cras in risus sed nisl vestibulum accumsan. Morbi quam ex, sodales sit amet felis vitae, ornare tincidunt ipsum. Sed vitae augue vel tellus volutpat pellentesque. Vivamus sit amet malesuada diam, vel fermentum enim.

Etiam quam odio, consequat id urna id, lobortis elementum nibh. Quisque tristique mi vel lobortis ornare. Vivamus in ornare orci. Nullam in tellus quis nulla tincidunt consectetur sit amet molestie justo. Ut volutpat tortor sapien, quis tincidunt nisl ullamcorper eget. Aenean quis commodo massa, sit amet rhoncus augue. Nullam ac felis a diam pretium efficitur quis et erat.

Cras nisi quam, suscipit a egestas eget, lacinia quis nisl. Phasellus eu urna dapibus purus fringilla lobortis. Proin sapien tellus, aliquet ultricies nisi sed, hendrerit semper arcu. Fusce vitae ultricies enim. Curabitur mattis elit vestibulum, rutrum lacus et, dictum ante. Phasellus in aliquet neque. Duis lobortis luctus posuere. Praesent eleifend vehicula ante, vitae luctus mi elementum in.

Etiam ornare neque quis ante porttitor sodales eget sit amet urna. Phasellus nec lacus eget lacus commodo viverra eget a ex. Nulla facilisi. In ornare pulvinar accumsan. Curabitur quis ullamcorper leo. Duis fermentum elementum est. Sed enim ipsum, tincidunt mollis odio ut, venenatis pulvinar nibh. Nulla aliquet nisi elit, vel auctor massa tempus in. Praesent tempus, mauris id efficitur fringilla, quam urna mollis sem, ut aliquet ipsum sapien vel massa. In hac habitasse platea dictumst. Nam commodo, quam sit amet scelerisque rhoncus, ante nibh vestibulum diam, id porta nibh velit eget lorem. Duis libero nulla, rhoncus ornare mattis eu, eleifend eu leo. Sed id magna arcu. Nulla facilisi. Vivamus consequat dolor et urna posuere, ut tempus augue sollicitudin. Aenean bibendum ut odio ut imperdiet.)"
        });

        return VStack {
            .spacing = 10.f,
            .children = children(
                Text {
                    .text = "Text Area",
                    .font = theme.fontDisplay,
                    .color = theme.colorTextPrimary,
                },
                TextArea {
                    .value = text,
                    .placeholder = "Start typing…",
                }.flex(1.f, 1.f, 0.f)
            ),
        }.padding(20.f);
    }
};

int main(int argc, char* argv[]) {
    Application app(argc, argv);
    auto& w = app.createWindow<Window>({
        .size = {480, 560},
        .title = "Flux — TextArea",
        .resizable = true,
    });

    w.registerAction("edit.copy", {.label = "Copy", .shortcut = shortcuts::Copy});
    w.registerAction("edit.cut", {.label = "Cut", .shortcut = shortcuts::Cut});
    w.registerAction("edit.paste", {.label = "Paste", .shortcut = shortcuts::Paste});
    w.registerAction("edit.selectAll", {.label = "Select All", .shortcut = shortcuts::SelectAll});
    w.registerAction("app.quit", {.label = "Quit", .shortcut = shortcuts::Quit, .isEnabled = [] { return true; }});

    w.setView<NotesEditor>();
    return app.exec();
}
