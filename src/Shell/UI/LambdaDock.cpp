#include "Shell/UI/LambdaDock.hpp"

#include <Flux/Core/Color.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/IconName.hpp>
#include <Flux/UI/Views/Icon.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>

namespace lambda_shell {
namespace {

flux::Color rgba(float r, float g, float b, float a) {
  return flux::Color{r, g, b, a};
}

flux::FillStyle gradient(flux::Color from, flux::Color to) {
  return flux::FillStyle::linearGradient(from, to, {0.f, 0.f}, {1.f, 1.f});
}

struct IconPalette {
  flux::Color from;
  flux::Color to;
  flux::Color ink;
};

IconPalette iconPalette(DockItem const& item) {
  if (item.kind == "launcher") return {rgba(0.96f, 0.97f, 0.99f, 1.f), rgba(0.80f, 0.84f, 0.92f, 1.f), rgba(0.13f, 0.20f, 0.33f, 1.f)};
  if (item.appId == "files") return {rgba(0.43f, 0.65f, 1.f, 1.f), rgba(0.16f, 0.50f, 1.f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.appId == "browser") return {rgba(0.37f, 0.72f, 1.f, 1.f), rgba(0.16f, 0.53f, 1.f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.appId == "terminal") return {rgba(0.16f, 0.18f, 0.27f, 1.f), rgba(0.05f, 0.07f, 0.14f, 1.f), rgba(0.75f, 0.90f, 1.f, 1.f)};
  if (item.appId == "settings") return {rgba(0.58f, 0.63f, 0.72f, 1.f), rgba(0.31f, 0.35f, 0.45f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.appId == "calendar") return {rgba(1.f, 1.f, 1.f, 1.f), rgba(0.92f, 0.94f, 0.98f, 1.f), rgba(0.90f, 0.29f, 0.24f, 1.f)};
  if (item.appId == "mail") return {rgba(0.44f, 0.71f, 1.f, 1.f), rgba(0.16f, 0.53f, 1.f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.appId == "music") return {rgba(0.79f, 0.50f, 0.90f, 1.f), rgba(0.48f, 0.25f, 1.f, 1.f), rgba(1.f, 1.f, 1.f, 1.f)};
  if (item.kind == "trash") return {rgba(0.80f, 0.84f, 0.89f, 1.f), rgba(0.58f, 0.64f, 0.74f, 1.f), rgba(0.10f, 0.15f, 0.25f, 1.f)};
  return {rgba(0.96f, 0.97f, 0.99f, 1.f), rgba(0.82f, 0.86f, 0.93f, 1.f), rgba(0.10f, 0.15f, 0.25f, 1.f)};
}

flux::IconName dockIconName(DockItem const& item) {
  if (item.kind == "launcher") return flux::IconName::Dashboard;
  if (item.appId == "files") return flux::IconName::FolderOpen;
  if (item.appId == "browser") return flux::IconName::Globe;
  if (item.appId == "terminal") return flux::IconName::Terminal;
  if (item.appId == "settings") return flux::IconName::Tune;
  if (item.appId == "calendar") return flux::IconName::CalendarToday;
  if (item.appId == "mail") return flux::IconName::Mail;
  if (item.appId == "music") return flux::IconName::LibraryMusic;
  if (item.kind == "trash") return flux::IconName::DeleteForever;
  return flux::IconName::Apps;
}

flux::Element dockIcon(DockItem item, bool hover, std::function<void()> onTap) {
  IconPalette const palette = iconPalette(item);
  float const lift = hover ? -5.f : 0.f;
  float const tileX = static_cast<float>((kDockCell - kDockIconTile) / 2);
  float const tileY = 0.f + lift;
  float const iconInset = static_cast<float>((kDockIconTile - kDockIconSize) / 2);
  std::vector<flux::Element> layers;
  layers.push_back(flux::Rectangle{}
      .size(static_cast<float>(kDockIconTile), static_cast<float>(kDockIconTile))
      .position(tileX, tileY)
      .fill(gradient(palette.from, palette.to))
      .stroke(flux::StrokeStyle::solid(rgba(1.f, 1.f, 1.f, 0.72f), 1.f))
      .cornerRadius(13.f));
  layers.push_back(flux::Icon{
      .name = dockIconName(item),
      .size = static_cast<float>(kDockIconSize),
      .weight = 820.f,
      .color = palette.ink,
  }.position(tileX + iconInset, tileY + iconInset));
  if (item.running) {
    layers.push_back(flux::Rectangle{}
        .size(5.f, 5.f)
        .position((static_cast<float>(kDockCell) - 5.f) * 0.5f, static_cast<float>(kDockCell) + 4.f)
        .fill(rgba(0.08f, 0.12f, 0.22f, item.focused ? 0.95f : 0.40f))
        .cornerRadius(2.5f));
  }
  auto element = flux::ZStack{
      .children = std::move(layers),
  }.size(static_cast<float>(kDockCell), static_cast<float>(kDockCell + 8));
  if (onTap) element = std::move(element).onTap(std::move(onTap));
  return element;
}

} // namespace

flux::Element LambdaDock::body() const {
  std::vector<flux::Element> children;
  children.reserve(props.items.size());
  for (std::size_t i = 0; i < props.items.size(); ++i) {
    DockItem const& item = props.items[i];
    if (item.kind == "separator") {
      children.push_back(flux::Rectangle{}
          .size(static_cast<float>(kDockSeparatorWidth), 30.f)
          .fill(rgba(1.f, 1.f, 1.f, 0.30f)));
      continue;
    }
    std::function<void()> onTap;
    if (item.kind == "launcher") {
      onTap = props.onOpenLauncher;
    } else if (props.onActivateItem) {
      onTap = [callback = props.onActivateItem, item] { callback(item); };
    }
    children.push_back(dockIcon(item, props.hoverIndex == static_cast<int>(i), std::move(onTap)));
  }
  return flux::HStack{
      .spacing = static_cast<float>(kDockGap),
      .alignment = flux::Alignment::Start,
      .children = std::move(children),
  }.padding(static_cast<float>(kDockPaddingY), static_cast<float>(kDockPaddingX),
            static_cast<float>(kDockPaddingY), static_cast<float>(kDockPaddingX))
   .size(static_cast<float>(props.width), static_cast<float>(dockHeight()))
   .fill(flux::Colors::transparent);
}

flux::Element LambdaDockSurface::body() const {
  return flux::ZStack{
      .children = flux::children(
          flux::Rectangle{}
              .size(static_cast<float>(props.width), static_cast<float>(dockHeight()))
              .fill(rgba(0.02f, 0.03f, 0.06f, 0.72f))
              .stroke(flux::StrokeStyle::solid(rgba(1.f, 1.f, 1.f, 0.16f), 0.8f))
              .cornerRadius(18.f),
          flux::Element{LambdaDock{props}}),
  }.size(static_cast<float>(props.width), static_cast<float>(dockHeight()));
}

} // namespace lambda_shell
