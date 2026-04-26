#include <Flux.hpp>
#include <Flux/Core/WindowUI.hpp>
#include <Flux/UI/Theme.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/For.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Rectangle.hpp>
#include <Flux/UI/Views/Show.hpp>
#include <Flux/UI/Views/Switch.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>
#include <vector>

using namespace flux;

namespace {

struct Message {
  int id = 0;
  std::string role;
  std::string text;

  bool operator==(Message const&) const = default;
};

std::vector<Message> seedMessages() {
  return {
      {1, "system", "Local model ready with v5 retained UI."},
      {2, "user", "Summarize the active document."},
      {3, "assistant", "The document is queued for a streamed response."},
  };
}

Element pill(Theme const& theme, std::string label, bool selected, std::function<void()> onTap) {
  return Text{
      .text = std::move(label),
      .font = Font::headline(),
      .color = selected ? Color::accent() : Color::primary(),
      .horizontalAlignment = HorizontalAlignment::Center,
  }.padding(theme.space3)
   .fill(selected ? Color::controlBackground() : Colors::transparent)
   .stroke(selected ? Color::accent() : Color::separator(), 1.f)
   .cornerRadius(theme.radiusFull)
   .onTap(std::move(onTap));
}

struct LambdaStudioRoot {
  Element body() const {
    Theme const& theme = useEnvironment<Theme>();
    auto tab = useState(0);
    auto connected = useState(true);
    auto messages = useState(seedMessages());

    return VStack{
        .spacing = theme.space5,
        .alignment = Alignment::Center,
        .children = children(
            Text{
                .text = "Lambda Studio",
                .font = Font::largeTitle(),
                .color = Color::primary(),
                .horizontalAlignment = HorizontalAlignment::Center,
            },
            HStack{
                .spacing = theme.space3,
                .alignment = Alignment::Center,
                .children = children(
                    pill(theme, "Chat", *tab == 0, [tab] { tab = 0; }),
                    pill(theme, "Models", *tab == 1, [tab] { tab = 1; }),
                    pill(theme, "Settings", *tab == 2, [tab] { tab = 2; }))},
            Show(
                connected.signal,
                [theme] {
                  return Text{
                      .text = "Runtime connected",
                      .font = Font::footnote(),
                      .color = Color::secondary(),
                  }.padding(theme.space2)
                   .fill(Color::controlBackground())
                   .cornerRadius(theme.radiusFull);
                },
                [theme] {
                  return Text{
                      .text = "Runtime offline",
                      .font = Font::footnote(),
                      .color = Color::secondary(),
                  }.padding(theme.space2)
                   .fill(Color::controlBackground())
                   .cornerRadius(theme.radiusFull);
                }),
            Switch(
                [tab] { return tab.get(); },
                std::vector{
                    Case(0, [theme, messages] {
                      return VStack{
                          .spacing = theme.space3,
                          .alignment = Alignment::Start,
                          .children = children(
                              Text{
                                  .text = "Conversation",
                                  .font = Font::title2(),
                                  .color = Color::primary(),
                              },
                              Element{For(
                                  messages.signal,
                                  [](Message const& message) { return message.id; },
                                  [theme](Message const& message) {
                                    return VStack{
                                        .spacing = theme.space1,
                                        .alignment = Alignment::Start,
                                        .children = children(
                                            Text{
                                                .text = message.role,
                                                .font = Font::footnote(),
                                                .color = Color::secondary(),
                                            },
                                            Text{
                                                .text = message.text,
                                                .font = Font::body(),
                                                .color = Color::primary(),
                                                .wrapping = TextWrapping::Wrap,
                                            }.width(520.f))}.padding(theme.space3)
                                           .fill(Color::controlBackground())
                                           .stroke(Color::separator(), 1.f)
                                           .cornerRadius(theme.radiusMedium);
                                  },
                                  theme.space3)}
                                  .height(260.f))};
                    }),
                    Case(1, [theme] {
                      return VStack{
                          .spacing = theme.space3,
                          .alignment = Alignment::Start,
                          .children = children(
                              Text{
                                  .text = "Models",
                                  .font = Font::title2(),
                                  .color = Color::primary(),
                              },
                              Text{
                                  .text = "qwen2.5-coder, llama-3.2, mistral-small",
                                  .font = Font::body(),
                                  .color = Color::secondary(),
                              })};
                    }),
                    Case(2, [theme, connected] {
                      return VStack{
                          .spacing = theme.space3,
                          .alignment = Alignment::Start,
                          .children = children(
                              Text{
                                  .text = "Settings",
                                  .font = Font::title2(),
                                  .color = Color::primary(),
                              },
                              Text{
                                  .text = *connected ? "Disconnect runtime" : "Connect runtime",
                                  .font = Font::body(),
                                  .color = Color::primary(),
                              }.padding(theme.space3)
                               .fill(Color::controlBackground())
                               .stroke(Color::separator(), 1.f)
                               .cornerRadius(theme.radiusFull)
                               .onTap([connected] { connected = !*connected; }))};
                    }),
                }))}
        .padding(theme.space7)
        .fill(Color::windowBackground())
        .environment(theme);
  }
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& window = app.createWindow<Window>({
      .size = {900, 700},
      .title = "Lambda Studio",
      .resizable = true,
  });

  window.setView<LambdaStudioRoot>();
  return app.exec();
}
