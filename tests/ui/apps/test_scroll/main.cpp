#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <sstream>
#include <string>
#include <vector>

using namespace flux;

struct ScrollTestRoot {
  auto body() const {
    std::vector<Element> rows;
    rows.reserve(40);
    for (int i = 1; i <= 40; ++i) {
      std::ostringstream line;
      line << "Row " << i << " — scroll harness";
      rows.push_back(
          Text{.text = line.str(),
               .font = {.size = 15.f, .weight = 400.f},
               .color = Color::rgb(28, 28, 36),
               .horizontalAlignment = HorizontalAlignment::Leading});
    }

    return VStack{
        .spacing = 0.f,
        .padding = 0.f,
        .children =
            {
                Text{.text = "Scroll test",
                     .font = {.size = 22.f, .weight = 700.f},
                     .color = Color::rgb(18, 18, 24),
                     .horizontalAlignment = HorizontalAlignment::Center,
                     .padding = 14.f,
                     .testFocusKey = "scroll-title"},
                ScrollView{
                    .axis = ScrollAxis::Vertical,
                    .flexGrow = 1.f,
                    .children =
                        {
                            VStack{
                                .spacing = 10.f,
                                .padding = 20.f,
                                .children = std::move(rows),
                            },
                        },
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow({
      .size = {440, 420},
      .title = "Flux UI test — scroll",
  });

  w.setView<ScrollTestRoot>();
  return app.exec();
}
