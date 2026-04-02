#include <Flux.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/TextArea.hpp>
#include <Flux/UI/Views/Spacer.hpp>
#include <Flux/UI/Views/VStack.hpp>

using namespace flux;

struct SidebarView {
  auto body() const {
    return VStack{
      .spacing = 8.f,
      .children = {
        Text {.text = "Sidebar", .font = {.size = 13.f, .weight = 600.f}},
        Spacer{}
      },
    };
  }
};

struct EditorView {
  auto body() const {
    auto text = useState(std::string{});

    return VStack{
        .spacing = 8.f,
        .children = {
            Text {
                .text = "Notes",
                .font = {.size = 13.f, .weight = 600.f},
            },
            TextArea {
                .value = text,
                .placeholder = "Start typing…",
                .onChange =
                    [](std::string const &v) {
                        std::fprintf(stderr, "length=%zu\n", v.size());
                    },
                .onEscape = [text](std::string const &) { text = ""; },
            },
        },
    };
  }
};

struct PreviewView {
    auto body() const {
      auto text = useState(std::string{});

      return VStack{
          .spacing = 8.f,
          .children = {
              Text {
                  .text = "Preview",
                  .font = {.size = 13.f, .weight = 600.f},
              },
              TextArea {
                  .value = text,
                  .placeholder = "Start typing…",
                  .onChange =
                      [](std::string const &v) {
                          std::fprintf(stderr, "length=%zu\n", v.size());
                      },
                  .onEscape = [text](std::string const &) { text = ""; },
              },
          },
      };
    }
  };

  struct AppView {
    auto body() const {
      return HStack{
        .spacing = 8.f,
        .padding = 8.f,
        .children = {
          Element{SidebarView()}.flex(0.f, 0.0f, 240.0f),
          Element{EditorView()}.flex(1.f),
          Element{PreviewView()}.flex(1.f),
        },
      };
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