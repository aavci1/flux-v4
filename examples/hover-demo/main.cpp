#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/ForEach.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>
#include <Flux/UI/Views/ZStack.hpp>

#include <functional>
#include <numeric>
#include <string>

using namespace flux;

struct ThreeStateButton {
  std::string label;
  std::function<void()> onTap;

  auto body() const {
    bool const hovered = useHover();
    bool const pressed = usePress();

    Color const bg = pressed ? Color::hex(0x2860B0) : hovered ? Color::hex(0x4A8BE0) : Color::hex(0x3A7BD5);

    return ZStack{
        .children = {
            Rectangle {
              .fill = FillStyle::solid(bg)
            }
            .height(44.f)
            .cursor(Cursor::Hand)
            .onTap(onTap)
            .cornerRadius(CornerRadius(10.f)),
            Text{
                .text = label,
                .font = {.size = 15.f, .weight = 600.f},
                .color = Color::hex(0xFFFFFF),
                .horizontalAlignment = HorizontalAlignment::Center,
                .verticalAlignment = VerticalAlignment::Center,
            }
            .padding(12.f),
        }
                };
  }
};

struct HoverListRow {
  int index = 0;

  auto body() const {
    bool const hovered = useHover();

    return ZStack{.children = {
                      Rectangle{
                          .fill = FillStyle::solid(hovered ? Color::hex(0xDFDFE8) : Color::hex(0xFFFFFF)),
                      }
                          .height(48.f)
                          .cursor(Cursor::Hand)
                          .cornerRadius(CornerRadius(8.f)),
                      Text{.text = "Item " + std::to_string(index) + (hovered ? "  ← pointer is here" : ""),
                           .font = {.size = 14.f},
                           .color = Color::hex(0x111118),
                       }
                          .padding(14.f),
                  }};
  }
};

struct HoverDemo {
  auto body() const {
    return ZStack{.children = {
                      Rectangle{.fill = FillStyle::solid(Color::hex(0xF2F2F7))},
                      VStack{
                          .spacing = 20.f,
                          .hAlign = HorizontalAlignment::Leading,
                          .children =
                              {
                                  Text{.text = "useHover / usePress demo",
                                       .font = {.size = 22.f, .weight = 700.f},
                                       .color = Color::hex(0x111118)},
                                  HStack{
                                      .spacing = 0.f,
                                      .children =
                                          {
                                              Text{.text = "Move over the button to see hover. "
                                                           "Click and hold to see press. "
                                                           "Drag outside while holding to confirm press stays active.",
                                                   .font = {.size = 14.f},
                                                   .color = Color::hex(0x6E6E80),
                                                   .wrapping = TextWrapping::Wrap}
                                                  .flex(1.f),
                                          },
                                  },
                                  ThreeStateButton{
                                      .label = "Hover / Click me",
                                      .onTap = [] {},
                                  },
                                  Text{.text = "Hover rows (no click needed):",
                                       .font = {.size = 14.f, .weight = 600.f},
                                       .color = Color::hex(0x111118)},
                                  ForEach<int>(
                                      [] {
                                        std::vector<int> v(5);
                                        std::iota(v.begin(), v.end(), 0);
                                        return v;
                                      }(),
                                      [](int i) -> Element { return Element{HoverListRow{.index = i}}; },
                                      6.f),
                              },
                      }.padding(32.f),
                  }};
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);
  auto& w = app.createWindow<Window>({
      .size = {520, 640},
      .title = "Flux — useHover / usePress demo",
      .resizable = true,
  });
  w.setView<HoverDemo>();
  return app.exec();
}
