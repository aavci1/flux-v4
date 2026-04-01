// Minimal layout tree for GetUi / bounds checks (reactive root matches other harness apps).
#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

using namespace flux;

struct LayoutTestRoot {
  auto body() const {
    return VStack{
        .spacing = 16.f,
        .padding = 24.f,
        .hAlign = HorizontalAlignment::Leading,
        .children =
            {
                Text{.text = "Layout test",
                     .font = {.size = 22.f, .weight = 700.f},
                     .color = Color::hex(0x111118),
                     .testFocusKey = "title"},
                HStack{
                    .spacing = 12.f,
                    .vAlign = VerticalAlignment::Center,
                    .children =
                        {
                            Rectangle{
                                .frame = {0.f, 0.f, 80.f, 48.f},
                                .cornerRadius = CornerRadius(8.f),
                                .fill = FillStyle::solid(Color::hex(0x3B82F6)),
                            },
                            Rectangle{
                                .frame = {0.f, 0.f, 80.f, 48.f},
                                .cornerRadius = CornerRadius(8.f),
                                .fill = FillStyle::solid(Color::hex(0x22C55E)),
                            },
                        },
                },
                Text{.text = "Hello from layout harness",
                     .font = {.size = 15.f, .weight = 400.f},
                     .color = Color::hex(0x6E6E80)},
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow({
      .size = {420, 360},
      .title = "Flux UI test — layout",
  });

  w.setView<LayoutTestRoot>();
  return app.exec();
}
