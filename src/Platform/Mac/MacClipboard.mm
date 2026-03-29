#include "Platform/Mac/MacClipboard.hpp"

#import <AppKit/AppKit.h>

#include <optional>
#include <string>

namespace flux {

std::optional<std::string> MacClipboard::readText() const {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSString* s = [pb stringForType:NSPasteboardTypeString];
  if (!s) {
    return std::nullopt;
  }
  const char* utf8 = [s UTF8String];
  if (!utf8) {
    return std::nullopt;
  }
  return std::string{utf8};
}

void MacClipboard::writeText(std::string text) {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  [pb clearContents];
  NSString* s = [NSString stringWithUTF8String:text.c_str()];
  if (s) {
    [pb setString:s forType:NSPasteboardTypeString];
  }
}

bool MacClipboard::hasText() const {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  return [pb availableTypeFromArray:@[NSPasteboardTypeString]] != nil;
}

} // namespace flux
