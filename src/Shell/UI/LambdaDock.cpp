#include "Shell/UI/LambdaDock.hpp"

#include "Shell/ShellAppRegistry.hpp"

#include <Flux/Core/Color.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Styles.hpp>
#include <Flux/UI/IconName.hpp>
#include <Flux/UI/Views/Image.hpp>
#include <Flux/UI/Views/Views.hpp>

#include <algorithm>
#include <memory>
#include <unordered_map>

using namespace flux;

namespace lambda_shell {
namespace {

std::string utf8(char32_t codepoint) {
  std::string out;
  if (codepoint <= 0x7Fu) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FFu) {
    out.push_back(static_cast<char>(0xC0u | (codepoint >> 6u)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  } else if (codepoint <= 0xFFFFu) {
    out.push_back(static_cast<char>(0xE0u | (codepoint >> 12u)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  } else {
    out.push_back(static_cast<char>(0xF0u | (codepoint >> 18u)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu)));
    out.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
  }
  return out;
}

std::string icon(IconName name) {
  return utf8(static_cast<char32_t>(name));
}

std::shared_ptr<Image> iconImage(std::string const& path) {
  if (path.empty()) return nullptr;
  static std::unordered_map<std::string, std::shared_ptr<Image>> cache;
  auto found = cache.find(path);
  if (found != cache.end()) return found->second;
  auto image = loadImage(path);
  cache.emplace(path, image);
  return image;
}

std::string iconKey(DockItem const& item) {
  if (!item.icon.empty()) return item.icon;
  return item.appId;
}

IconName dockIconName(DockItem const& item) {
  std::string const key = iconKey(item);
  if (item.kind == "launcher") return IconName::Dashboard;
  if (shellAppIdMatches("files", key)) return IconName::FolderOpen;
  if (shellAppIdMatches("browser", key)) return IconName::Globe;
  if (shellAppIdMatches("terminal", key)) return IconName::Terminal;
  if (shellAppIdMatches("settings", key)) return IconName::Tune;
  if (key == "calendar") return IconName::CalendarToday;
  if (key == "mail") return IconName::Mail;
  if (key == "music") return IconName::LibraryMusic;
  if (item.kind == "trash") return IconName::DeleteForever;
  return IconName::Apps;
}

Element dockIconAt(std::size_t index,
                         DockItem const& item,
                         Signal<std::vector<DockItem>> const& items,
                         bool hover,
                         std::function<void()> onTap) {
  float const lift = hover ? -5.f : 0.f;
  Reactive::Bindable<bool> running{[items, index] {
    auto const& dockItems = items();
    return index < dockItems.size() && dockItems[index].running;
  }};
  Reactive::Bindable<bool> focused{[items, index] {
    auto const& dockItems = items();
    return index < dockItems.size() && dockItems[index].focused;
  }};

  float const slotWidth = static_cast<float>(kDockCell);
  float const slotHeight = static_cast<float>(kDockSlotHeight);
  float const iconSize = static_cast<float>(kDockIconSize);
  float const iconDotGap = static_cast<float>(kDockIconDotGap);
  float const slotMargin = static_cast<float>(kDockSlotMargin);
  float const dotBelowPad = static_cast<float>(kDockDotBelowPad);
  float const iconInsetX = (slotWidth - iconSize) * 0.5f;

  std::vector<Element> iconLayers;
  auto image = iconImage(item.iconPath);
  if (image) {
    iconLayers.push_back(Element{views::Image{
        .source = std::move(image),
        .fillMode = ImageFillMode::Fit,
    }}.size(iconSize, iconSize).position(iconInsetX, lift));
  } else {
    iconLayers.push_back(Text{
        .text = icon(dockIconName(item)),
        .font = Font{.family = "Material Symbols Rounded", .size = 36.f, .weight = 680.f},
        .color = Color(1.f, 1.f, 1.f, 0.92f),
        .horizontalAlignment = HorizontalAlignment::Center,
        .verticalAlignment = VerticalAlignment::Center,
    }.size(iconSize, iconSize).position(iconInsetX, lift));
  }

  float const dotSize = static_cast<float>(kDockDotSize);
  Reactive::Bindable<float> dotOpacity{[running] { return running.evaluate() ? 1.f : 0.f; }};
  Element dotLayer = Element{Rectangle{}}
      .width(dotSize)
      .height(dotSize)
      .fill(Reactive::Bindable<FillStyle>{[focused] {
        return focused.evaluate() ? FillStyle::solid(Color(0.35f, 0.72f, 1.f, 1.f))
                                  : FillStyle::solid(Color(1.f, 1.f, 1.f, 0.72f));
      }})
      .cornerRadius(3.f)
      .opacity(dotOpacity);

  auto element = VStack{
      .spacing = iconDotGap,
      .alignment = Alignment::Center,
      .children = children(
          ZStack{.children = std::move(iconLayers)}.size(slotWidth, iconSize),
          HStack{
              .alignment = Alignment::Center,
              .justifyContent = JustifyContent::Center,
              .children = children(std::move(dotLayer)),
          }
              .size(slotWidth, dotSize)),
  }
      .padding(slotMargin, 0.f, dotBelowPad, 0.f)
      .size(slotWidth, slotHeight);
  if (onTap) element = std::move(element).onTap(std::move(onTap));
  return element;
}

Element dockSeparator() {
  float const thickness = static_cast<float>(kDockSeparatorWidth);
  float const height = static_cast<float>(kDockIconSize);
  return Rectangle{}
      .size(thickness, height)
      .fill(FillStyle::solid(Color{1.f, 1.f, 1.f, 0.30f}));
}

} // namespace

Element LambdaDock::body() const {
  auto const items = props.items;
  auto const width = props.width;
  auto const onOpenLauncher = props.onOpenLauncher;
  auto const onActivateItem = props.onActivateItem;
  int const hoverIndex = props.hoverIndex;

  std::vector<Element> children;
  std::vector<DockItem> const snapshot = items.peek();
  children.reserve(snapshot.size());
  for (std::size_t i = 0; i < snapshot.size(); ++i) {
    DockItem const& item = snapshot[i];
    if (item.kind == "separator") {
      children.push_back(dockSeparator());
      continue;
    }
    std::function<void()> onTap;
        if (item.kind == "launcher") {
            onTap = onOpenLauncher;
        } else if (onActivateItem) {
            onTap = [callback = onActivateItem, item] { callback(item); };
        }
        bool const hover = hoverIndex >= 0 && static_cast<int>(i) == hoverIndex;
    children.push_back(dockIconAt(i, item, items, hover, std::move(onTap)));
  }

  return HStack{
      .spacing = static_cast<float>(kDockGap),
      .alignment = Alignment::Center,
      .children = std::move(children),
  }
      .padding(kDockPaddingTop, kDockPaddingX, kDockPaddingBottom, kDockPaddingX)
      .size(Reactive::Bindable<float>{[width] { return static_cast<float>(width.evaluate()); }},
            static_cast<float>(dockHeight()));
}

} // namespace lambda_shell
