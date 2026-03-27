#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>
#import <CoreGraphics/CoreGraphics.h>

#include "Graphics/CoreTextSystem.hpp"

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

TextAttribute resolveAttribute(TextAttribute const& base, TextAttribute const& run) {
  TextAttribute a = run;
  if (a.fontFamily.empty()) {
    a.fontFamily = base.fontFamily;
  }
  if (a.fontSize <= 0.f) {
    a.fontSize = base.fontSize;
  }
  if (a.fontWeight <= 0.f) {
    a.fontWeight = base.fontWeight;
  }
  return a;
}

TextAttribute baseDefaults() {
  TextAttribute b;
  b.fontFamily = kDefaultFontFamily;
  b.fontSize = kDefaultFontSize;
  b.fontWeight = kDefaultFontWeight;
  b.italic = false;
  b.color = Colors::black;
  return b;
}

void accumulateInheritance(std::vector<TextAttribute>& out, AttributedString const& text) {
  out.clear();
  out.resize(text.runs.size());
  TextAttribute inherited = baseDefaults();
  for (std::size_t i = 0; i < text.runs.size(); ++i) {
    TextAttribute const& r = text.runs[i].attr;
    out[i] = resolveAttribute(inherited, r);
    if (!r.fontFamily.empty()) {
      inherited.fontFamily = r.fontFamily;
    }
    if (r.fontSize > 0.f) {
      inherited.fontSize = r.fontSize;
    }
    if (r.fontWeight > 0.f) {
      inherited.fontWeight = r.fontWeight;
    }
    inherited.italic = r.italic;
    inherited.color = r.color;
  }
}

CTFontRef createCTFont(TextAttribute const& attr) {
  NSString* fam = [NSString stringWithUTF8String:attr.fontFamily.c_str()];
  if (!fam) {
    fam = @".AppleSystemUIFont";
  }
  CGFloat const wTrait = ctWeightTraitFromCss(attr.fontWeight);
  NSDictionary* traits = @{
    (id)kCTFontSymbolicTrait : @(attr.italic ? kCTFontItalicTrait : 0),
    (id)kCTFontWeightTrait : @(wTrait),
  };
  NSDictionary* descDict = @{
    (id)kCTFontFamilyNameAttribute : fam,
    (id)kCTFontTraitsAttribute : traits,
  };
  CTFontDescriptorRef fd = CTFontDescriptorCreateWithAttributes((__bridge CFDictionaryRef)descDict);
  CTFontRef font = CTFontCreateWithFontDescriptor(fd, static_cast<CGFloat>(attr.fontSize), nullptr);
  CFRelease(fd);
  return font;
}

CFAttributedStringRef createCFAttributed(AttributedString const& text,
                                         std::vector<TextAttribute> const& resolved) {
  NSString* ns = [NSString stringWithUTF8String:text.utf8.c_str()];
  if (!ns) {
    ns = @"";
  }
  NSMutableAttributedString* mas =
      [[NSMutableAttributedString alloc] initWithString:ns attributes:@{}];

  for (std::size_t ri = 0; ri < text.runs.size(); ++ri) {
    auto const& run = text.runs[ri];
    TextAttribute const& a = resolved[ri];
    NSRange range = NSMakeRange(run.start, run.end - run.start);
    CTFontRef font = createCTFont(a);
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

  return (__bridge_retained CFAttributedStringRef)mas;
}

CFAttributedStringRef createCFAttributedPlain(std::string_view utf8, TextAttribute const& attr) {
  AttributedString as;
  as.utf8 = std::string(utf8);
  as.runs.push_back({0, static_cast<std::uint32_t>(utf8.size()), attr});
  std::vector<TextAttribute> resolved;
  accumulateInheritance(resolved, as);
  return createCFAttributed(as, resolved);
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
  TextAttribute a;
  a.fontFamily = key.family;
  a.fontSize = 12.f;
  a.fontWeight = key.weight;
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

Size CoreTextSystem::measure(AttributedString const& text, float maxWidth) {
  if (text.utf8.empty()) {
    return {};
  }
  validateRuns(text);
  std::vector<TextAttribute> resolved;
  accumulateInheritance(resolved, text);
  CFAttributedStringRef cf = createCFAttributed(text, resolved);
  Size s = measureCF(cf, maxWidth);
  CFRelease(cf);
  return s;
}

Size CoreTextSystem::measurePlain(std::string_view utf8, TextAttribute const& attr, float maxWidth) {
  if (utf8.empty()) {
    return {};
  }
  CFAttributedStringRef cf = createCFAttributedPlain(utf8, attr);
  Size s = measureCF(cf, maxWidth);
  CFRelease(cf);
  return s;
}

std::shared_ptr<TextRun> CoreTextSystem::shapePlain(std::string_view utf8, TextAttribute const& attr,
                                                    float maxWidth) {
  AttributedString as;
  as.utf8 = std::string(utf8);
  as.runs.push_back({0, static_cast<std::uint32_t>(utf8.size()), attr});
  return shape(as, maxWidth);
}

std::shared_ptr<TextRun> CoreTextSystem::shape(AttributedString const& text, float maxWidth) {
  (void)maxWidth; // reserved for future line-breaking; single-line layout uses CTLine (no width wrap)
  if (text.utf8.empty()) {
    return std::make_shared<TextRun>(TextRun{});
  }
  validateRuns(text);
  std::vector<TextAttribute> resolved;
  accumulateInheritance(resolved, text);

  CFAttributedStringRef cfAttr = createCFAttributed(text, resolved);
  // Single-line TextRun: build a CTLine directly. CTFrame + path uses Quartz Y-up frame coords; using
  // CTRun positions without CTFrameGetLineOrigins misplaces glyphs (baseline/punctuation vs canvas Y-down).
  CTLineRef line = CTLineCreateWithAttributedString(cfAttr);
  CFRelease(cfAttr);
  if (!line) {
    return std::make_shared<TextRun>(TextRun{});
  }
  double lineAscent = 0, lineDescent = 0, lineLeading = 0;
  double const lineWidth = CTLineGetTypographicBounds(line, &lineAscent, &lineDescent, &lineLeading);

  CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
  CFIndex runCount = CFArrayGetCount(glyphRuns);

  std::vector<std::uint16_t> allGids;
  std::vector<Point> allPos;
  bool styleFromFirst = false;
  std::uint32_t fontId = 0;
  float fontSize = 0.f;
  Color color = Colors::black;

  for (CFIndex ri = 0; ri < runCount; ++ri) {
    CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, ri);
    CFIndex glyphCount = CTRunGetGlyphCount(run);
    if (glyphCount <= 0) {
      continue;
    }

    NSDictionary* attrs = (__bridge NSDictionary*)CTRunGetAttributes(run);
    CTFontRef ctFont = (__bridge CTFontRef)attrs[(id)kCTFontAttributeName];
    CGColorRef cgColor = (__bridge CGColorRef)attrs[(id)kCTForegroundColorAttributeName];
    Color runColor = colorFromCGColor(cgColor);

    CGFloat ctFontSize = CTFontGetSize(ctFont);
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

    std::uint32_t const fid = resolveFontId(fam, cssWeight, italic);

    if (!styleFromFirst) {
      fontId = fid;
      fontSize = static_cast<float>(ctFontSize);
      color = runColor;
      styleFromFirst = true;
    }

    std::vector<std::uint16_t> gids(static_cast<std::size_t>(glyphCount));
    std::vector<CGPoint> cpos(static_cast<std::size_t>(glyphCount));
    CTRunGetGlyphs(run, CFRangeMake(0, 0), reinterpret_cast<CGGlyph*>(gids.data()));
    CTRunGetPositions(run, CFRangeMake(0, 0), cpos.data());

    std::size_t const base = allGids.size();
    allGids.resize(base + static_cast<std::size_t>(glyphCount));
    allPos.resize(base + static_cast<std::size_t>(glyphCount));
    for (CFIndex gi = 0; gi < glyphCount; ++gi) {
      allGids[base + static_cast<std::size_t>(gi)] = gids[static_cast<std::size_t>(gi)];
      // CTRun positions are in Quartz line space (Y up from baseline). Flux / MetalCanvas use Y down.
      allPos[base + static_cast<std::size_t>(gi)] =
          Point{static_cast<float>(cpos[static_cast<std::size_t>(gi)].x),
                -static_cast<float>(cpos[static_cast<std::size_t>(gi)].y)};
    }
  }

  CFRelease(line);

  auto out = std::make_shared<TextRun>();
  out->measuredSize =
      Size{static_cast<float>(lineWidth),
           static_cast<float>(lineAscent + lineDescent + lineLeading)};
  out->fontId = fontId;
  out->fontSize = fontSize;
  out->color = color;
  out->glyphIds = std::move(allGids);
  out->positions = std::move(allPos);
  out->ascent = static_cast<float>(lineAscent);
  out->descent = static_cast<float>(lineDescent);
  return out;
}

} // namespace flux
