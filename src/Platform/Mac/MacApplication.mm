#include "Core/PlatformApplication.hpp"

#include <Flux/Core/KeyCodes.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#import <Cocoa/Cocoa.h>

namespace flux {
class MacApplication;
}

@interface FluxAppDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate>
@property(nonatomic, assign) flux::MacApplication* owner;
- (void)fluxMenuAction:(id)sender;
@end

namespace flux {

namespace {

NSString* ns(std::string const& text) {
  NSString* out = [NSString stringWithUTF8String:text.c_str()];
  return out ? out : @"";
}

std::string appNameFromBundle() {
  NSString* name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
  if (!name || name.length == 0) {
    name = [[NSProcessInfo processInfo] processName];
  }
  return name ? std::string([name UTF8String]) : std::string("Flux");
}

std::string roleActionName(MenuRole role) {
  switch (role) {
  case MenuRole::AppPreferences:
    return "settings";
  case MenuRole::EditUndo:
    return "undo";
  case MenuRole::EditRedo:
    return "redo";
  case MenuRole::EditCut:
    return "cut";
  case MenuRole::EditCopy:
    return "copy";
  case MenuRole::EditPaste:
    return "paste";
  case MenuRole::EditDelete:
    return "delete";
  case MenuRole::EditSelectAll:
    return "select-all";
  default:
    return {};
  }
}

bool systemHandledRole(MenuRole role) {
  switch (role) {
  case MenuRole::AppAbout:
  case MenuRole::AppHide:
  case MenuRole::AppHideOthers:
  case MenuRole::AppShowAll:
  case MenuRole::AppQuit:
  case MenuRole::WindowMinimize:
  case MenuRole::WindowZoom:
  case MenuRole::WindowFullscreen:
  case MenuRole::WindowBringAllToFront:
    return true;
  default:
    return false;
  }
}

std::string roleLabel(MenuRole role, std::string const& appName) {
  switch (role) {
  case MenuRole::AppAbout:
    return "About " + appName;
  case MenuRole::AppPreferences:
    return "Settings...";
  case MenuRole::AppHide:
    return "Hide " + appName;
  case MenuRole::AppHideOthers:
    return "Hide Others";
  case MenuRole::AppShowAll:
    return "Show All";
  case MenuRole::AppQuit:
    return "Quit " + appName;
  case MenuRole::EditUndo:
    return "Undo";
  case MenuRole::EditRedo:
    return "Redo";
  case MenuRole::EditCut:
    return "Cut";
  case MenuRole::EditCopy:
    return "Copy";
  case MenuRole::EditPaste:
    return "Paste";
  case MenuRole::EditDelete:
    return "Delete";
  case MenuRole::EditSelectAll:
    return "Select All";
  case MenuRole::WindowMinimize:
    return "Minimize";
  case MenuRole::WindowZoom:
    return "Zoom";
  case MenuRole::WindowFullscreen:
    return "Enter Full Screen";
  case MenuRole::WindowBringAllToFront:
    return "Bring All to Front";
  case MenuRole::HelpSearch:
    return "Search";
  default:
    return {};
  }
}

Shortcut roleShortcut(MenuRole role) {
  switch (role) {
  case MenuRole::AppPreferences:
    return Shortcut{keys::Comma, Modifiers::Meta};
  case MenuRole::AppHide:
    return Shortcut{keys::H, Modifiers::Meta};
  case MenuRole::AppHideOthers:
    return Shortcut{keys::H, Modifiers::Meta | Modifiers::Alt};
  case MenuRole::AppQuit:
    return shortcuts::Quit;
  case MenuRole::EditUndo:
    return shortcuts::Undo;
  case MenuRole::EditRedo:
    return shortcuts::Redo;
  case MenuRole::EditCut:
    return shortcuts::Cut;
  case MenuRole::EditCopy:
    return shortcuts::Copy;
  case MenuRole::EditPaste:
    return shortcuts::Paste;
  case MenuRole::EditSelectAll:
    return shortcuts::SelectAll;
  case MenuRole::WindowMinimize:
    return Shortcut{keys::M, Modifiers::Meta};
  case MenuRole::WindowFullscreen:
    return Shortcut{keys::F, Modifiers::Meta | Modifiers::Ctrl};
  default:
    return {};
  }
}

NSString* keyEquivalent(KeyCode key) {
  switch (key) {
  case keys::A: return @"a";
  case keys::B: return @"b";
  case keys::C: return @"c";
  case keys::D: return @"d";
  case keys::E: return @"e";
  case keys::F: return @"f";
  case keys::G: return @"g";
  case keys::H: return @"h";
  case keys::I: return @"i";
  case keys::J: return @"j";
  case keys::K: return @"k";
  case keys::L: return @"l";
  case keys::M: return @"m";
  case keys::N: return @"n";
  case keys::O: return @"o";
  case keys::P: return @"p";
  case keys::Q: return @"q";
  case keys::R: return @"r";
  case keys::S: return @"s";
  case keys::T: return @"t";
  case keys::U: return @"u";
  case keys::V: return @"v";
  case keys::W: return @"w";
  case keys::X: return @"x";
  case keys::Y: return @"y";
  case keys::Z: return @"z";
  case keys::Comma: return @",";
  case keys::Period: return @".";
  case keys::Slash: return @"/";
  case keys::Return: return @"\r";
  default:
    return @"";
  }
}

NSEventModifierFlags modifierMask(Modifiers modifiers) {
  NSEventModifierFlags out = 0;
  if (any(modifiers & Modifiers::Meta)) {
    out |= NSEventModifierFlagCommand;
  }
  if (any(modifiers & Modifiers::Shift)) {
    out |= NSEventModifierFlagShift;
  }
  if (any(modifiers & Modifiers::Ctrl)) {
    out |= NSEventModifierFlagControl;
  }
  if (any(modifiers & Modifiers::Alt)) {
    out |= NSEventModifierFlagOption;
  }
  return out;
}

NSURL* directoryURL(NSSearchPathDirectory directory) {
  NSArray<NSURL*>* urls = [[NSFileManager defaultManager] URLsForDirectory:directory
                                                                  inDomains:NSUserDomainMask];
  NSURL* base = urls.firstObject;
  if (!base) {
    return nil;
  }
  NSString* appName = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
  if (!appName || appName.length == 0) {
    appName = [[NSProcessInfo processInfo] processName];
  }
  NSURL* appDir = [base URLByAppendingPathComponent:(appName ? appName : @"Flux")];
  [[NSFileManager defaultManager] createDirectoryAtURL:appDir
                           withIntermediateDirectories:YES
                                            attributes:nil
                                                 error:nil];
  return appDir;
}

} // namespace

class MacApplication final : public PlatformApplication {
public:
  void initialize() override {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    delegate_ = [[FluxAppDelegate alloc] init];
    delegate_.owner = this;
    [NSApp setDelegate:delegate_];
  }

  void setMenuBar(MenuBar const& menu, MenuActionDispatcher dispatcher) override {
    dispatcher_ = std::move(dispatcher);
    claimedShortcuts_.clear();

    std::string const appName = menu.appName.empty() ? appNameFromBundle() : menu.appName;
    std::vector<MenuItem> menus = menu.menus;
    if (menus.empty() || menus.front().role != MenuRole::Submenu) {
      menus.insert(menus.begin(), MenuItem::submenu(appName, {
                                     MenuItem::standard(MenuRole::AppAbout),
                                     MenuItem::separator(),
                                     MenuItem::standard(MenuRole::AppHide),
                                     MenuItem::standard(MenuRole::AppHideOthers),
                                     MenuItem::standard(MenuRole::AppShowAll),
                                     MenuItem::separator(),
                                     MenuItem::standard(MenuRole::AppQuit),
                                 }));
    }

    NSMenu* main = [[NSMenu alloc] initWithTitle:@""];
    for (MenuItem const& top : menus) {
      if (top.role != MenuRole::Submenu) {
        continue;
      }
      NSMenuItem* topItem = [[NSMenuItem alloc] initWithTitle:ns(top.label)
                                                       action:nil
                                                keyEquivalent:@""];
      NSMenu* submenu = [[NSMenu alloc] initWithTitle:ns(top.label)];
      submenu.delegate = delegate_;
      for (MenuItem const& child : top.children) {
        addMenuItem(submenu, child, appName);
      }
      topItem.submenu = submenu;
      [main addItem:topItem];
    }
    [NSApp setMainMenu:main];
  }

  void setTerminateHandler(std::function<void()> handler) override {
    terminateHandler_ = std::move(handler);
  }

  std::unordered_set<ShortcutKey, ShortcutKeyHash> menuClaimedShortcuts() const override {
    return claimedShortcuts_;
  }

  void revalidateMenuItems(std::function<bool(std::string const&)> isEnabled) override {
    isEnabled_ = std::move(isEnabled);
  }

  std::string userDataDir() const override {
    NSURL* url = directoryURL(NSApplicationSupportDirectory);
    return url ? std::string(url.path.UTF8String) : std::string{};
  }

  std::string cacheDir() const override {
    NSURL* url = directoryURL(NSCachesDirectory);
    return url ? std::string(url.path.UTF8String) : std::string{};
  }

  bool dispatchMenuItem(NSMenuItem* item) {
    MenuRole const role = static_cast<MenuRole>(item.tag);
    switch (role) {
    case MenuRole::AppAbout:
      [NSApp orderFrontStandardAboutPanel:nil];
      return true;
    case MenuRole::AppHide:
      [NSApp hide:nil];
      return true;
    case MenuRole::AppHideOthers:
      [NSApp hideOtherApplications:nil];
      return true;
    case MenuRole::AppShowAll:
      [NSApp unhideAllApplications:nil];
      return true;
    case MenuRole::AppQuit:
      [NSApp terminate:nil];
      return true;
    case MenuRole::WindowMinimize:
      [[NSApp keyWindow] performMiniaturize:nil];
      return true;
    case MenuRole::WindowZoom:
      [[NSApp keyWindow] performZoom:nil];
      return true;
    case MenuRole::WindowFullscreen:
      [[NSApp keyWindow] toggleFullScreen:nil];
      return true;
    case MenuRole::WindowBringAllToFront:
      [NSApp arrangeInFront:nil];
      return true;
    default:
      break;
    }

    NSString* action = [item representedObject];
    if (!action) {
      return false;
    }
    return dispatcher_ && dispatcher_(action.UTF8String);
  }

  bool validateMenuItem(NSMenuItem* item) const {
    MenuRole const role = static_cast<MenuRole>(item.tag);
    if (systemHandledRole(role)) {
      return true;
    }
    NSString* action = [item representedObject];
    if (!action) {
      return true;
    }
    return isEnabled_ && isEnabled_(action.UTF8String);
  }

  void handleShouldTerminate() {
    if (terminateHandler_) {
      terminateHandler_();
    }
  }

private:
  void addMenuItem(NSMenu* menu, MenuItem const& item, std::string const& appName) {
    if (item.role == MenuRole::Separator) {
      [menu addItem:[NSMenuItem separatorItem]];
      return;
    }

    if (item.role == MenuRole::Submenu) {
      NSMenuItem* submenuItem = [[NSMenuItem alloc] initWithTitle:ns(item.label)
                                                          action:nil
                                                   keyEquivalent:@""];
      NSMenu* submenu = [[NSMenu alloc] initWithTitle:ns(item.label)];
      submenu.delegate = delegate_;
      for (MenuItem const& child : item.children) {
        addMenuItem(submenu, child, appName);
      }
      submenuItem.submenu = submenu;
      [menu addItem:submenuItem];
      return;
    }

    std::string actionName = item.actionName;
    if (actionName.empty()) {
      actionName = roleActionName(item.role);
    }
    std::string title = item.label.empty() ? roleLabel(item.role, appName) : item.label;
    Shortcut shortcut = item.shortcut;
    if (shortcut.key == 0 && shortcut.modifiers == Modifiers::None) {
      shortcut = roleShortcut(item.role);
    }
    NSString* key = keyEquivalent(shortcut.key);
    NSMenuItem* nsItem = [[NSMenuItem alloc] initWithTitle:ns(title)
                                                    action:@selector(fluxMenuAction:)
                                             keyEquivalent:(key ? key : @"")];
    nsItem.target = delegate_;
    nsItem.tag = static_cast<NSInteger>(item.role);
    if (!actionName.empty()) {
      nsItem.representedObject = ns(actionName);
    }
    if (shortcut.matches(shortcut.key, shortcut.modifiers)) {
      nsItem.keyEquivalentModifierMask = modifierMask(shortcut.modifiers);
      claimedShortcuts_.insert(ShortcutKey{.key = shortcut.key, .modifiers = shortcut.modifiers});
    }
    [menu addItem:nsItem];
  }

  __strong FluxAppDelegate* delegate_{nil};
  MenuActionDispatcher dispatcher_;
  std::function<void()> terminateHandler_;
  std::function<bool(std::string const&)> isEnabled_;
  std::unordered_set<ShortcutKey, ShortcutKeyHash> claimedShortcuts_;
};

namespace detail {
std::unique_ptr<PlatformApplication> createPlatformApplication() {
  return std::make_unique<MacApplication>();
}
} // namespace detail

} // namespace flux

@implementation FluxAppDelegate

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  (void)sender;
  return YES;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
  (void)sender;
  if (self.owner) {
    self.owner->handleShouldTerminate();
  }
  return NSTerminateNow;
}

- (void)fluxMenuAction:(id)sender {
  NSMenuItem* item = [sender isKindOfClass:[NSMenuItem class]] ? sender : nil;
  if (item && self.owner) {
    self.owner->dispatchMenuItem(item);
  }
}

- (BOOL)validateMenuItem:(NSMenuItem*)menuItem {
  return self.owner ? self.owner->validateMenuItem(menuItem) : YES;
}

@end
