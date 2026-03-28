#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>
#import <CoreGraphics/CoreGraphics.h>

#include "Graphics/CoreTextSystem.hpp"

#include <Flux/Graphics/TextLayout.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace flux {

namespace {

constexpr char const* kDefaultFontFamily = ".AppleSystemUIFont";
constexpr float kDefaultFontSize = 14.f;
constexpr float kDefaultFontWeight = 400.f;
constexpr float kPadPx = 1.f;

struct FontKey {
  std::string family;
  float weight = 400.f;
  bool italic = false;
  bool operator==(FontKey const& o) const noexcept {
    return family == o.family && weight == o.weight && italic == o.italic;
  }
};

struct FontKeyHash {
  std::size_t operator()(FontKey const& k) const noexcept {
    std::size_t h = std::hash<std::string>{}(k.family);
    h ^= std::hash<float>{}(k.weight) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(k.italic) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

// Core Text weight trait is roughly in [-1, 1]; regular ≈ 0, bold ≈ 0.4.
CGFloat ctWeightTraitFromCss(float w) {
  if (w <= 0.f) {
    return 0.0;
  }
  if (w < 450.f) {
    return 0.0;
  }
  if (w < 600.f) {
    return 0.23;
  }
  return 0.4;
}

Color colorFromCGColor(CGColorRef cg) {
  if (!cg) {
    return Colors::black;
  }
  const size_t n = CGColorGetNumberOfComponents(cg);
  const CGFloat* c = CGColorGetComponents(cg);
  if (n >= 4) {
    return Color{static_cast<float>(c[0]), static_cast<float>(c[1]), static_cast<float>(c[2]),
                 static_cast<float>(c[3])};
  }
  if (n >= 2) {
    return Color{static_cast<float>(c[0]), static_cast<float>(c[0]), static_cast<float>(c[0]),
                 static_cast<float>(c[1])};
  }
  return Colors::black;
}

void validateRuns(AttributedString const& text) {
  if (text.utf8.empty()) {
    return;
  }
  if (text.runs.empty()) {
    throw std::invalid_argument("AttributedString: runs must cover the string (empty runs)");
  }
  std::uint32_t const n = static_cast<std::uint32_t>(text.utf8.size());
  if (text.runs[0].start != 0 || text.runs.back().end != n) {
    throw std::invalid_argument("AttributedString: runs must cover full utf8 range");
  }
  for (std::size_t i = 0; i < text.runs.size(); ++i) {
    auto const& r = text.runs[i];
    if (r.start >= r.end || r.end > n) {
      throw std::invalid_argument("AttributedString: invalid run range");
    }
    if (i + 1 < text.runs.size() && r.end != text.runs[i + 1].start) {
      throw std::invalid_argument("AttributedString: runs must be contiguous");
    }
  }
}

struct ResolvedStyle {
  Font font;
  Color color;
};

Font resolveFont(Font const& base, Font const& run) {
  Font a = run;
  if (a.family.empty()) {
    a.family = base.family;
  }
  if (a.size <= 0.f) {
    a.size = base.size;
  }
  if (a.weight <= 0.f) {
    a.weight = base.weight;
  }
  return a;
}

Font baseDefaultsFont() {
  Font b;
  b.family = kDefaultFontFamily;
  b.size = kDefaultFontSize;
  b.weight = kDefaultFontWeight;
  b.italic = false;
  return b;
}

void accumulateInheritance(std::vector<ResolvedStyle>& out, AttributedString const& text) {
  out.clear();
  out.resize(text.runs.size());
  Font inherited = baseDefaultsFont();
  for (std::size_t i = 0; i < text.runs.size(); ++i) {
    AttributedRun const& run = text.runs[i];
    Font const& rf = run.font;
    out[i].font = resolveFont(inherited, rf);
    out[i].color = run.color;
    if (!rf.family.empty()) {
      inherited.family = rf.family;
    }
    if (rf.size > 0.f) {
      inherited.size = rf.size;
    }
    if (rf.weight > 0.f) {
      inherited.weight = rf.weight;
    }
    inherited.italic = rf.italic;
  }
}

CTFontRef createCTFont(Font const& attr) {
  NSString* fam = [NSString stringWithUTF8String:attr.family.c_str()];
  if (!fam) {
    fam = @".AppleSystemUIFont";
  }
  CGFloat const wTrait = ctWeightTraitFromCss(attr.weight);
  NSDictionary* traits = @{
    (id)kCTFontSymbolicTrait : @(attr.italic ? kCTFontItalicTrait : 0),
    (id)kCTFontWeightTrait : @(wTrait),
  };
  NSDictionary* descDict = @{
    (id)kCTFontFamilyNameAttribute : fam,
    (id)kCTFontTraitsAttribute : traits,
  };
  CTFontDescriptorRef fd = CTFontDescriptorCreateWithAttributes((__bridge CFDictionaryRef)descDict);
  CTFontRef font = CTFontCreateWithFontDescriptor(fd, static_cast<CGFloat>(attr.size), nullptr);
  CFRelease(fd);
  return font;
}

/// `AttributedString` run ranges are UTF-8 byte offsets; `NSMutableAttributedString` uses UTF-16 indices.
static size_t utf8Advance(char const* s, std::size_t len, std::size_t i, std::uint32_t* outCp) {
  if (i >= len) {
    return 0;
  }
  auto const u = static_cast<unsigned char>(s[i]);
  if (u < 0x80) {
    *outCp = u;
    return 1;
  }
  if (i + 1 < len && (u & 0xE0) == 0xC0) {
    auto const u1 = static_cast<unsigned char>(s[i + 1]);
    if ((u1 & 0xC0) != 0x80) {
      return 0;
    }
    *outCp = (static_cast<std::uint32_t>(u & 0x1F) << 6) | (u1 & 0x3F);
    return 2;
  }
  if (i + 2 < len && (u & 0xF0) == 0xE0) {
    auto const u1 = static_cast<unsigned char>(s[i + 1]);
    auto const u2 = static_cast<unsigned char>(s[i + 2]);
    if ((u1 & 0xC0) != 0x80 || (u2 & 0xC0) != 0x80) {
      return 0;
    }
    *outCp = (static_cast<std::uint32_t>(u & 0x0F) << 12) | (static_cast<std::uint32_t>(u1 & 0x3F) << 6) |
             (u2 & 0x3F);
    return 3;
  }
  if (i + 3 < len && (u & 0xF8) == 0xF0) {
    auto const u1 = static_cast<unsigned char>(s[i + 1]);
    auto const u2 = static_cast<unsigned char>(s[i + 2]);
    auto const u3 = static_cast<unsigned char>(s[i + 3]);
    if ((u1 & 0xC0) != 0x80 || (u2 & 0xC0) != 0x80 || (u3 & 0xC0) != 0x80) {
      return 0;
    }
    *outCp = (static_cast<std::uint32_t>(u & 0x07) << 18) | (static_cast<std::uint32_t>(u1 & 0x3F) << 12) |
             (static_cast<std::uint32_t>(u2 & 0x3F) << 6) | (u3 & 0x3F);
    return 4;
  }
  return 0;
}

static NSUInteger utf16UnitsForCodepoint(std::uint32_t cp) {
  return cp > 0xFFFF ? 2u : 1u;
}

/// Maps half-open UTF-8 byte range `[bStart, bEnd)` to an `NSRange` in UTF-16 (Foundation string units).
static NSRange utf8ByteRangeToNSRange(char const* utf8, std::size_t utf8Len, std::uint32_t bStart,
                                      std::uint32_t bEnd) {
  if (bEnd > utf8Len) {
    bEnd = static_cast<std::uint32_t>(utf8Len);
  }
  if (bStart > bEnd) {
    bStart = bEnd;
  }
  NSUInteger u16Start = 0;
  std::size_t i = 0;
  while (i < utf8Len && i < bStart) {
    std::uint32_t cp = 0;
    std::size_t const adv = utf8Advance(utf8, utf8Len, i, &cp);
    if (adv == 0) {
      break;
    }
    u16Start += utf16UnitsForCodepoint(cp);
    i += adv;
  }
  NSUInteger u16End = u16Start;
  while (i < utf8Len && i < bEnd) {
    std::uint32_t cp = 0;
    std::size_t const adv = utf8Advance(utf8, utf8Len, i, &cp);
    if (adv == 0) {
      break;
    }
    u16End += utf16UnitsForCodepoint(cp);
    i += adv;
  }
  return NSMakeRange(u16Start, u16End - u16Start);
}

static void applyParagraphStyleToMutable(NSMutableAttributedString* mas, TextLayoutOptions const& options) {
  if (!mas || [mas length] == 0) {
    return;
  }
  CTLineBreakMode lineBreak = kCTLineBreakByWordWrapping;
  if (options.wrapping == TextWrapping::WrapAnywhere) {
    lineBreak = kCTLineBreakByCharWrapping;
  } else {
    lineBreak = kCTLineBreakByWordWrapping;
  }

  CTParagraphStyleSetting settings[3];
  std::size_t n = 0;
  settings[n].spec = kCTParagraphStyleSpecifierLineBreakMode;
  settings[n].valueSize = sizeof(lineBreak);
  settings[n].value = &lineBreak;
  ++n;

  CGFloat minLh = 0;
  CGFloat maxLh = 0;
  if (options.lineHeight > 0.f) {
    minLh = static_cast<CGFloat>(options.lineHeight);
    maxLh = static_cast<CGFloat>(options.lineHeight);
    settings[n].spec = kCTParagraphStyleSpecifierMinimumLineHeight;
    settings[n].valueSize = sizeof(minLh);
    settings[n].value = &minLh;
    ++n;
    settings[n].spec = kCTParagraphStyleSpecifierMaximumLineHeight;
    settings[n].valueSize = sizeof(maxLh);
    settings[n].value = &maxLh;
    ++n;
  }

  CTParagraphStyleRef ps = CTParagraphStyleCreate(settings, static_cast<CFIndex>(n));
  NSDictionary* paraAttrs = @{ (id)kCTParagraphStyleAttributeName : (__bridge id)ps };
  [mas addAttributes:paraAttrs range:NSMakeRange(0, [mas length])];
  CFRelease(ps);
}

CFAttributedStringRef createCFAttributed(AttributedString const& text,
                                         std::vector<ResolvedStyle> const& resolved,
                                         TextLayoutOptions const& options) {
  NSString* ns = [NSString stringWithUTF8String:text.utf8.c_str()];
  if (!ns) {
    ns = @"";
  }
  NSMutableAttributedString* mas =
      [[NSMutableAttributedString alloc] initWithString:ns attributes:@{}];

  char const* const bytes = text.utf8.c_str();
  std::size_t const byteLen = text.utf8.size();

  for (std::size_t ri = 0; ri < text.runs.size(); ++ri) {
    auto const& run = text.runs[ri];
    ResolvedStyle const& a = resolved[ri];
    NSRange range = utf8ByteRangeToNSRange(bytes, byteLen, run.start, run.end);
    CTFontRef font = createCTFont(a.font);
    CGFloat rgba[4] = {a.color.r, a.color.g, a.color.b, a.color.a};
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGColorRef cg = CGColorCreate(cs, rgba);
    CGColorSpaceRelease(cs);
    NSDictionary* attrs = @{
      (id)kCTFontAttributeName : (__bridge id)font,
      (id)kCTForegroundColorAttributeName : (__bridge id)cg,
    };
    [mas addAttributes:attrs range:range];
    CGColorRelease(cg);
    CFRelease(font);
  }

  applyParagraphStyleToMutable(mas, options);
  return (__bridge_retained CFAttributedStringRef)mas;
}

CFAttributedStringRef createCFAttributedPlain(std::string_view utf8, Font const& font, Color const& color,
                                              TextLayoutOptions const& options) {
  AttributedString as;
  as.utf8 = std::string(utf8);
  as.runs.push_back({0, static_cast<std::uint32_t>(utf8.size()), font, color});
  std::vector<ResolvedStyle> resolved;
  accumulateInheritance(resolved, as);
  return createCFAttributed(as, resolved, options);
}

/// Resolves `fontId` / style from a Core Text run’s font (same mapping as the previous single-line shaper).
static void styleFromCTFont(CTFontRef ctFont, CGColorRef cgColor, CoreTextSystem& sys, std::uint32_t& outFontId,
                            float& outFontSize, Color& outColor) {
  outColor = colorFromCGColor(cgColor);
  outFontSize = static_cast<float>(CTFontGetSize(ctFont));
  CTFontDescriptorRef fd = CTFontCopyFontDescriptor(ctFont);
  CFDictionaryRef traitDict =
      static_cast<CFDictionaryRef>(CTFontDescriptorCopyAttribute(fd, kCTFontTraitsAttribute));
  CFRelease(fd);
  bool italic = false;
  CGFloat weightTrait = 0.0;
  if (traitDict) {
    NSDictionary* traits = (__bridge NSDictionary*)traitDict;
    NSNumber* sym = traits[(id)kCTFontSymbolicTrait];
    if (sym) {
      italic = ([sym unsignedIntValue] & kCTFontItalicTrait) != 0;
    }
    NSNumber* wt = traits[(id)kCTFontWeightTrait];
    if (wt) {
      weightTrait = [wt floatValue];
    }
    CFRelease(traitDict);
  }
  CFStringRef famRef = CTFontCopyFamilyName(ctFont);
  NSString* famNs = (__bridge NSString*)famRef;
  std::string fam = famNs ? [famNs UTF8String] : std::string(kDefaultFontFamily);
  if (famRef) {
    CFRelease(famRef);
  }
  float cssWeight = 400.f;
  if (weightTrait >= 0.35) {
    cssWeight = 700.f;
  } else if (weightTrait >= 0.15) {
    cssWeight = 600.f;
  }
  outFontId = sys.resolveFontId(fam, cssWeight, italic);
}

/// One `CTRun` → `PlacedRun`: glyph positions are relative to the run’s baseline-left; `origin` is baseline-left
/// in layout space (top-left origin, Y down). `frameHeight` is the CT frame path height (Quartz, Y up).
static void appendPlacedRunFromCTRun(CTRunRef run, CGPoint lineOrigin, CGFloat frameHeight, CoreTextSystem& sys,
                                     TextLayout& layout) {
  CFIndex const glyphCount = CTRunGetGlyphCount(run);
  if (glyphCount <= 0) {
    return;
  }
  NSDictionary* attrs = (__bridge NSDictionary*)CTRunGetAttributes(run);
  CTFontRef ctFont = (__bridge CTFontRef)attrs[(id)kCTFontAttributeName];
  CGColorRef cgColor = (__bridge CGColorRef)attrs[(id)kCTForegroundColorAttributeName];

  std::uint32_t fontId = 0;
  float fontSize = 0.f;
  Color color = Colors::black;
  styleFromCTFont(ctFont, cgColor, sys, fontId, fontSize, color);

  std::vector<std::uint16_t> gids(static_cast<std::size_t>(glyphCount));
  std::vector<CGPoint> cpos(static_cast<std::size_t>(glyphCount));
  CTRunGetGlyphs(run, CFRangeMake(0, 0), reinterpret_cast<CGGlyph*>(gids.data()));
  CTRunGetPositions(run, CFRangeMake(0, 0), cpos.data());

  CGFloat ascent = 0, descent = 0, leading = 0;
  double const runWidth = CTRunGetTypographicBounds(run, CFRangeMake(0, 0), &ascent, &descent, &leading);

  TextLayout::PlacedRun placed{};
  placed.run.fontId = fontId;
  placed.run.fontSize = fontSize;
  placed.run.color = color;
  placed.run.glyphIds = std::move(gids);
  placed.run.ascent = static_cast<float>(ascent);
  placed.run.descent = static_cast<float>(descent);
  placed.run.width = static_cast<float>(runWidth);
  placed.run.positions.resize(static_cast<std::size_t>(glyphCount));
  // LTR: positions are anchored to glyph index 0. For RTL (`kCTRunStatusRightToLeft`), index 0 is not
  // necessarily the visual left edge; relative dx/dy would need a different anchor when RTL is supported.
  for (CFIndex gi = 0; gi < glyphCount; ++gi) {
    float const dx = static_cast<float>(cpos[static_cast<std::size_t>(gi)].x - cpos[0].x);
    float const dy =
        -static_cast<float>(cpos[static_cast<std::size_t>(gi)].y - cpos[0].y); // Quartz Y up → canvas Y down
    placed.run.positions[static_cast<std::size_t>(gi)] = Point{dx, dy};
  }

  CGFloat const baselineX = lineOrigin.x + cpos[0].x;
  CGFloat const baselineY = lineOrigin.y + cpos[0].y;
  placed.origin.x = static_cast<float>(baselineX);
  placed.origin.y = static_cast<float>(frameHeight - baselineY);

  layout.runs.push_back(std::move(placed));
}

} // namespace

struct CoreTextSystem::Impl {
  std::unordered_map<FontKey, std::uint32_t, FontKeyHash> fontIds_;
  std::vector<CTFontRef> fontById_;

  std::uint32_t fontIdForKey(FontKey const& key, CTFontRef font) {
    auto it = fontIds_.find(key);
    if (it != fontIds_.end()) {
      CFRelease(font);
      return it->second;
    }
    std::uint32_t const id = static_cast<std::uint32_t>(fontById_.size());
    fontIds_[key] = id;
    fontById_.push_back(font);
    return id;
  }

  CTFontRef fontAtSize(std::uint32_t id, float size) const {
    if (id >= fontById_.size()) {
      return nullptr;
    }
    CTFontRef base = fontById_[id];
    return CTFontCreateCopyWithAttributes(base, static_cast<CGFloat>(size), nullptr, nullptr);
  }

  ~Impl() {
    for (CTFontRef f : fontById_) {
      if (f) {
        CFRelease(f);
      }
    }
  }
};

CoreTextSystem::CoreTextSystem() : d(std::make_unique<Impl>()) {}

CoreTextSystem::~CoreTextSystem() = default;

std::uint32_t CoreTextSystem::resolveFontId(std::string_view fontFamily, float weight, bool italic) {
  FontKey key;
  key.family = std::string(fontFamily.empty() ? kDefaultFontFamily : fontFamily);
  key.weight = weight > 0.f ? weight : kDefaultFontWeight;
  key.italic = italic;
  Font a;
  a.family = key.family;
  a.size = 12.f;
  a.weight = key.weight;
  a.italic = key.italic;
  CTFontRef font = createCTFont(a);
  return d->fontIdForKey(key, font);
}

std::vector<std::uint8_t> CoreTextSystem::rasterizeGlyph(std::uint32_t fontId, std::uint16_t glyphId,
                                                        float size, std::uint32_t& outWidth,
                                                        std::uint32_t& outHeight, Point& outBearing) {
  outWidth = 0;
  outHeight = 0;
  outBearing = {0, 0};
  CTFontRef font = d->fontAtSize(fontId, size);
  if (!font) {
    return {};
  }
  CGGlyph g = glyphId;
  CGRect bounds = CGRectZero;
  CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationHorizontal, &g, &bounds, 1);
  if (bounds.size.width <= 0 || bounds.size.height <= 0) {
    CFRelease(font);
    return {};
  }

  CGFloat const pad = kPadPx;
  CGFloat const ascent = CTFontGetAscent(font);
  CGFloat const descent = CTFontGetDescent(font); // positive below baseline
  // Top of glyph ink in glyph space (Y up from baseline): must not use font ascent alone — some glyphs
  // extend above the font ascent metric; then pad+ascent places the baseline too high and the ascenders
  // clip above the bitmap (top half missing after upload).
  CGFloat const glyphTop = bounds.origin.y + bounds.size.height;
  // Bitmap must fit the full line box or the glyph bbox, whichever is taller.
  double const minBoxH = static_cast<double>(pad + ascent + descent + pad);
  std::uint32_t const bw =
      static_cast<std::uint32_t>(std::ceil(static_cast<double>(bounds.size.width + pad * 2.f)));
  std::uint32_t const bh = static_cast<std::uint32_t>(
      std::max(std::ceil(static_cast<double>(bounds.size.height + pad * 2.f)), std::ceil(minBoxH)));
  if (bw == 0 || bh == 0) {
    CFRelease(font);
    return {};
  }

  std::vector<std::uint8_t> grayBuf(static_cast<std::size_t>(bw) * bh);
  CGColorSpaceRef grayCs = CGColorSpaceCreateDeviceGray();
  CGContextRef ctx =
      CGBitmapContextCreate(grayBuf.data(), bw, bh, 8, bw, grayCs, kCGImageAlphaNone);
  CGColorSpaceRelease(grayCs);
  if (!ctx) {
    CFRelease(font);
    return {};
  }

  CGContextSetShouldAntialias(ctx, true);
  CGContextSetShouldSmoothFonts(ctx, true);
  CGContextSetGrayFillColor(ctx, 1, 1);
  CGContextFillRect(ctx, CGRectMake(0, 0, bw, bh));
  CGContextSetGrayFillColor(ctx, 0, 1);

  // Default CGBitmapContext: origin bottom-left, Y up. Do not flip the CTM for text — CTFontDrawGlyphs and
  // CTFontGetBoundingRectsForGlyphs both use glyph space (baseline origin, Y up); a flipped CTM desyncs
  // outlines from the bbox used for ox/oy and causes per-glyph vertical drift vs line layout.
  CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
  CGFloat const ox = static_cast<CGFloat>(pad) - bounds.origin.x;
  // Top of ink at y = bh - pad (device space); baseline is glyphTop below that.
  CGFloat const baselineY = static_cast<CGFloat>(bh) - pad - glyphTop;
  CGContextSetTextPosition(ctx, ox, baselineY);
  CGPoint const z = CGPointZero;
  CTFontDrawGlyphs(font, &g, &z, 1, ctx);

  CGContextRelease(ctx);
  CFRelease(font);

  // `replaceRegion` row 0 must be the top of the image (Metal / UIKit-style, y down). CGBitmapContext’s
  // first row in memory is *not* always the bottom scanline in practice for this 8-bit gray context:
  // reversing (bh-1-row) put the glyph’s feet at texture row 0 and looked upside-down in the atlas PNG.
  // Copy rows in order so row 0 stays aligned with Core Graphics’ top-of-image scanline → Metal row 0.
  std::vector<std::uint8_t> r8(static_cast<std::size_t>(bw) * bh);
  for (std::uint32_t row = 0; row < bh; ++row) {
    std::uint8_t const* src = grayBuf.data() + static_cast<std::size_t>(row) * bw;
    std::uint8_t* dst = r8.data() + static_cast<std::size_t>(row) * bw;
    for (std::uint32_t col = 0; col < bw; ++col) {
      dst[col] = static_cast<std::uint8_t>(255 - src[col]);
    }
  }

  outWidth = bw;
  outHeight = bh;
  outBearing.x = static_cast<float>(ox);
  outBearing.y = static_cast<float>(pad + glyphTop);
  return r8;
}

static Size measureCF(CFAttributedStringRef attrStr, float maxWidth) {
  CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(attrStr);
  if (!fs) {
    return {};
  }
  CGSize constraints = CGSizeMake(maxWidth > 0.f ? static_cast<CGFloat>(maxWidth) : CGFLOAT_MAX, CGFLOAT_MAX);
  CFRange fitRange;
  CGSize sz = CTFramesetterSuggestFrameSizeWithConstraints(fs, CFRangeMake(0, 0), nullptr, constraints, &fitRange);
  CFRelease(fs);
  return Size{static_cast<float>(sz.width), static_cast<float>(sz.height)};
}

Size CoreTextSystem::measure(AttributedString const& text, float maxWidth, TextLayoutOptions const& options) {
  if (text.utf8.empty()) {
    return {};
  }
  if (options.maxLines > 0) {
    std::shared_ptr<TextLayout> const L = layout(text, maxWidth, options);
    return L ? L->measuredSize : Size{};
  }
  validateRuns(text);
  std::vector<ResolvedStyle> resolved;
  accumulateInheritance(resolved, text);
  CFAttributedStringRef cf = createCFAttributed(text, resolved, options);
  Size s = measureCF(cf, maxWidth);
  CFRelease(cf);
  return s;
}

Size CoreTextSystem::measure(std::string_view utf8, Font const& font, Color const& color, float maxWidth,
                             TextLayoutOptions const& options) {
  if (utf8.empty()) {
    return {};
  }
  if (options.maxLines > 0) {
    std::shared_ptr<TextLayout> const L = layout(utf8, font, color, maxWidth, options);
    return L ? L->measuredSize : Size{};
  }
  CFAttributedStringRef cf = createCFAttributedPlain(utf8, font, color, options);
  Size s = measureCF(cf, maxWidth);
  CFRelease(cf);
  return s;
}

std::shared_ptr<TextLayout> CoreTextSystem::layout(std::string_view utf8, Font const& font, Color const& color,
                                                   float maxWidth, TextLayoutOptions const& options) {
  AttributedString as;
  as.utf8 = std::string(utf8);
  as.runs.push_back({0, static_cast<std::uint32_t>(utf8.size()), font, color});
  return layout(as, maxWidth, options);
}

std::shared_ptr<TextLayout> CoreTextSystem::layout(AttributedString const& text, float maxWidth,
                                                    TextLayoutOptions const& options) {
  auto out = std::make_shared<TextLayout>();
  if (text.utf8.empty()) {
    return out;
  }
  validateRuns(text);
  std::vector<ResolvedStyle> resolved;
  accumulateInheritance(resolved, text);

  CFAttributedStringRef cfAttr = createCFAttributed(text, resolved, options);
  CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(cfAttr);
  CFRelease(cfAttr);
  if (!fs) {
    return out;
  }

  CGSize const constraints =
      CGSizeMake(maxWidth > 0.f ? static_cast<CGFloat>(maxWidth) : CGFLOAT_MAX, CGFLOAT_MAX);
  CFRange fitRange{};
  CGSize sz = CTFramesetterSuggestFrameSizeWithConstraints(fs, CFRangeMake(0, 0), nullptr, constraints,
                                                             &fitRange);

  CGFloat const fw = std::max(static_cast<CGFloat>(sz.width), static_cast<CGFloat>(1e-6));
  CGFloat const fh = std::max(static_cast<CGFloat>(sz.height), static_cast<CGFloat>(1e-6));
  out->measuredSize = Size{static_cast<float>(fw), static_cast<float>(fh)};

  CGPathRef path = CGPathCreateWithRect(CGRectMake(0, 0, fw, fh), nullptr);
  CTFrameRef frame = CTFramesetterCreateFrame(fs, CFRangeMake(0, 0), path, nullptr);
  CFRelease(path);
  CFRelease(fs);
  if (!frame) {
    return out;
  }

  CFArrayRef lines = CTFrameGetLines(frame);
  CFIndex const lineCount = lines ? CFArrayGetCount(lines) : 0;
  std::vector<CGPoint> origins(static_cast<std::size_t>(std::max(lineCount, CFIndex{0})));
  if (lineCount > 0) {
    CTFrameGetLineOrigins(frame, CFRangeMake(0, lineCount), origins.data());
  }

  for (CFIndex li = 0; li < lineCount; ++li) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(lines, li);
    CGPoint const lineOrigin = origins[static_cast<std::size_t>(li)];
    CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
    CFIndex const runCount = CFArrayGetCount(glyphRuns);
    for (CFIndex ri = 0; ri < runCount; ++ri) {
      CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, ri);
      appendPlacedRunFromCTRun(run, lineOrigin, fh, *this, *out);
    }
  }

  CFRelease(frame);

  recomputeTextLayoutMetrics(*out);
  if (options.maxLines > 0) {
    trimTextLayoutToMaxLines(*out, options.maxLines, true);
  }
  return out;
}

} // namespace flux
