#include <Flux.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <sstream>
#include <string>

using namespace flux;

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow({
      .size = {420, 560},
      .title = "Flux — Scroll demo",
  });

  std::vector<Element> rows;
  rows.reserve(40);
  for (int i = 1; i <= 40; ++i) {
    std::ostringstream line;
    line << "Row " << i << " — drag or scroll wheel / trackpad";
    rows.push_back(
        Text{.text = line.str(),
             .font = {.size = 16.f, .weight = 420.f},
             .color = Color::rgb(28, 28, 36),
             .horizontalAlignment = HorizontalAlignment::Leading});
  }

  w.setView(VStack{
      .spacing = 0.f,
      .children =
          {
              Text{.text = "ScrollView",
                   .font = {.size = 28.f, .weight = 600.f},
                   .color = Color::rgb(18, 18, 24),
                   .horizontalAlignment = HorizontalAlignment::Center,
               }
                  .padding(16.f),
              ScrollView{
                  .axis = ScrollAxis::Vertical,
                  .flexGrow = 1.f,
                  .children =
                      {
                          VStack{
                              .spacing = 10.f,
                              .children = std::move(rows),
                          }.padding(20.f),
                      },
              },
          },
  });

  return app.exec();
}
