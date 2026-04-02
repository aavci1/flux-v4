#include <Flux.hpp>
#include <Flux/UI/UI.hpp>
#include <Flux/UI/Views/Button.hpp>
#include <Flux/UI/Views/HStack.hpp>
#include <Flux/UI/Views/Popover.hpp>
#include <Flux/UI/Views/ScrollView.hpp>
#include <Flux/UI/Views/Text.hpp>
#include <Flux/UI/Views/VStack.hpp>

#include <string>
#include <vector>

using namespace flux;

namespace pal {
constexpr Color titleC = Color::hex(0x111118);
constexpr Color bodyC = Color::hex(0x6E6E80);
constexpr Color accent = Color::hex(0x3A7BD5);
} // namespace pal

struct PopoverDemoRoot {
  auto body() const {
    auto showArrow = useState<bool>(true);
    auto dismissOutside = useState<bool>(true);
    auto [showPopover, hidePopover, popoverOpen] = usePopover();

    std::vector<Element> scrollChildren;

    auto addSection = [&](char const* heading) {
      scrollChildren.push_back(
          Text{.text = heading,
               .font = {.size = 13.f, .weight = 600.f},
               .color = pal::bodyC,
           }
              .padding(8.f));
    };

    addSection("Placement");
    scrollChildren.push_back(
        HStack{
            .spacing = 0.f,
            .children =
                {
                    Text{.text = "Scroll so triggers sit near window edges to see flip.",
                         .font = {.size = 12.f, .weight = 400.f},
                         .color = pal::bodyC,
                         .wrapping = TextWrapping::Wrap,
                     }
                        .padding(8.f)
                        .flex(1.f),
                },
        });

    auto addPlacementButton = [&](char const* label, PopoverPlacement placement) {
      scrollChildren.push_back(Button{
          .label = label,
          .variant = ButtonVariant::Secondary,
          .onTap = [=] {
            showPopover(Popover{
                .content = Element{VStack{
                    .spacing = 8.f,
                    .hAlign = HorizontalAlignment::Leading,
                    .children =
                        {
                            Text{.text = std::string(label),
                                 .font = {.size = 15.f, .weight = 600.f},
                                 .color = pal::titleC},
                            HStack{
                                .spacing = 0.f,
                                .children =
                                    {
                                        Text{.text = "Placement follows preference when space allows.",
                                             .font = {.size = 13.f, .weight = 400.f},
                                             .color = pal::bodyC,
                                             .wrapping = TextWrapping::Wrap}
                                            .flex(1.f),
                                    },
                            },
                            Button{
                                .label = "Close",
                                .variant = ButtonVariant::Secondary,
                                .onTap = hidePopover,
                            },
                        },
                }},
                .placement = placement,
                .arrow = *showArrow,
                .maxSize = Size{260.f, 200.f},
                .backdropColor = Colors::transparent,
                .dismissOnEscape = true,
                .dismissOnOutsideTap = *dismissOutside,
            });
          },
      });
    };

    addPlacementButton("Below", PopoverPlacement::Below);
    addPlacementButton("Above", PopoverPlacement::Above);
    addPlacementButton("End (right in LTR)", PopoverPlacement::End);
    addPlacementButton("Start (left in LTR)", PopoverPlacement::Start);

    addSection("Options");
    scrollChildren.push_back(HStack{
        .spacing = 12.f,
        .vAlign = VerticalAlignment::Center,
        .children =
            {
                Text{.text = "Arrow",
                     .font = {.size = 14.f, .weight = 500.f},
                     .color = pal::titleC},
                Button{
                    .label = *showArrow ? "On" : "Off",
                    .variant = ButtonVariant::Ghost,
                    .onTap = [=] { showArrow = !*showArrow; },
                },
            },
    });
    scrollChildren.push_back(HStack{
        .spacing = 12.f,
        .vAlign = VerticalAlignment::Center,
        .children =
            {
                Text{.text = "Dismiss outside tap",
                     .font = {.size = 14.f, .weight = 500.f},
                     .color = pal::titleC},
                Button{
                    .label = *dismissOutside ? "On" : "Off",
                    .variant = ButtonVariant::Ghost,
                    .onTap = [=] { dismissOutside = !*dismissOutside; },
                },
            },
    });

    addSection("Anchor tracking (scroll)");
    for (int i = 0; i < 8; ++i) {
      scrollChildren.push_back(
          Text{.text = "Spacer row — scroll the list",
               .font = {.size = 13.f, .weight = 400.f},
               .color = pal::bodyC,
           }
              .padding(6.f));
    }
    scrollChildren.push_back(Button{
        .label = "Below — middle of scroll",
        .variant = ButtonVariant::Primary,
        .accentColor = pal::accent,
        .onTap = [=] {
          showPopover(Popover{
              .content = Element{VStack{
                  .spacing = 8.f,
                  .hAlign = HorizontalAlignment::Leading,
                  .children =
                      {
                          Text{.text = "Popover anchored to this button.",
                               .font = {.size = 17.f, .weight = 600.f},
                               .color = pal::titleC},
                          HStack{
                              .spacing = 0.f,
                              .children =
                                  {
                                      Text{
                                          .text = "ScrollView keeps layout rects updated; anchor follows the trigger.",
                                          .font = {.size = 13.f, .weight = 400.f},
                                          .color = pal::bodyC,
                                          .wrapping = TextWrapping::Wrap,
                                      }
                                          .flex(1.f),
                                  },
                          },
                          Button{.label = "OK", .onTap = hidePopover},
                      },
              }},
              .placement = PopoverPlacement::Below,
              .arrow = *showArrow,
              .maxSize = Size{280.f, 220.f},
              .backdropColor = Colors::transparent,
              .dismissOnEscape = true,
              .dismissOnOutsideTap = *dismissOutside,
          });
        },
    });
    for (int i = 0; i < 8; ++i) {
      scrollChildren.push_back(
          Text{.text = "Spacer row — scroll the list",
               .font = {.size = 13.f, .weight = 400.f},
               .color = pal::bodyC,
           }
              .padding(6.f));
    }
    scrollChildren.push_back(Button{
        .label = "Below — near bottom (may flip Above)",
        .variant = ButtonVariant::Primary,
        .accentColor = pal::accent,
        .onTap = [=] {
          showPopover(Popover{
              .content = Element{VStack{
                  .spacing = 8.f,
                  .hAlign = HorizontalAlignment::Leading,
                  .children =
                      {
                          Text{.text = "Flip test",
                               .font = {.size = 17.f, .weight = 600.f},
                               .color = pal::titleC},
                          HStack{
                              .spacing = 0.f,
                              .children =
                                  {
                                      Text{
                                          .text = "If there is not enough room below the anchor, placement flips to Above.",
                                          .font = {.size = 13.f, .weight = 400.f},
                                          .color = pal::bodyC,
                                          .wrapping = TextWrapping::Wrap,
                                      }
                                          .flex(1.f),
                                  },
                          },
                          Button{.label = "OK", .onTap = hidePopover},
                      },
              }},
              .placement = PopoverPlacement::Below,
              .arrow = *showArrow,
              .maxSize = Size{280.f, 220.f},
              .backdropColor = Colors::transparent,
              .dismissOnEscape = true,
              .dismissOnOutsideTap = *dismissOutside,
          });
        },
    });

    return VStack{
        .spacing = 0.f,
        .children =
            {
                Text{.text = "Popover demo",
                     .font = {.size = 22.f, .weight = 700.f},
                     .color = pal::titleC,
                     .horizontalAlignment = HorizontalAlignment::Center,
                 }
                    .padding(16.f),
                Text{.text = popoverOpen ? "Popover visible" : "Popover hidden",
                     .font = {.size = 13.f, .weight = 500.f},
                     .color = pal::bodyC,
                     .horizontalAlignment = HorizontalAlignment::Center,
                 }
                    .padding(8.f),
                ScrollView{
                    .axis = ScrollAxis::Vertical,
                    .flexGrow = 1.f,
                    .children =
                        {
                            VStack{
                                .spacing = 10.f,
                                .children = std::move(scrollChildren),
                            }.padding(20.f),
                        },
                },
            },
    };
  }
};

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  auto& w = app.createWindow({
      .size = {420, 620},
      .title = "Flux — Popover demo",
  });

  w.setView(PopoverDemoRoot{});

  return app.exec();
}
