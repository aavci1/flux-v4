#include <Flux.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextArea.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/VStack.hpp>

using namespace flux;

struct SidebarView {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    return VStack{
      .spacing = 8.f,
      .children = children(
        Text {.text = "Sidebar", .style = theme.typeLabel, .color = theme.colorTextSecondary},
        Spacer{}
      ),
    };
  }
};

struct EditorView {
  auto body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto text = useState(std::string{});

    return VStack{
        .spacing = 8.f,
        .children = children(
            Text {
                .text = "Notes",
                .style = theme.typeHeading,
                .color = theme.colorTextPrimary,
            },
            TextArea{
                .value = text,
                .placeholder = "Start typing…",
                .onChange =
                    [](std::string const &v) {
                      std::fprintf(stderr, "length=%zu\n", v.size());
                    },
                .onEscape = [text](std::string const &) { text = ""; },
            }
                .flex(1.f, 1.f, 0.f)
        ),
    };
  }
};

struct PreviewView {
    auto body() const {
      Theme const& theme = useEnvironment<Theme>();
      auto text = useState(std::string{});

      return VStack{
          .spacing = 8.f,
          .children = children(
              Text {
                  .text = "Preview",
                  .style = theme.typeHeading,
                  .color = theme.colorTextPrimary,
              },
              TextArea{
                  .value = text,
                  .placeholder = "Start typing…",
                  .onChange =
                      [](std::string const &v) {
                        std::fprintf(stderr, "length=%zu\n", v.size());
                      },
                  .onEscape = [text](std::string const &) { text = ""; },
              }
                  .flex(1.f, 1.f, 0.f)
          ),
      };
    }
  };

  struct AppView {
    auto body() const {
      return HStack{
        .spacing = 8.f,
        .children = children(
          Element{SidebarView()}.flex(0.f, 0.0f, 240.0f),
          Element{EditorView()}.flex(1.f),
          Element{PreviewView()}.flex(1.f)
        ),
      }.padding(8.f);
    }
  };

int main(int argc, char *argv[]) {
  Application app(argc, argv);

  auto &w = app.createWindow<Window>({
      .size = {800, 600},
      .title = "Flux Notes",
  });

  w.setView<AppView>();

  return app.exec();
}