#import <Foundation/Foundation.h>
#import <CoreText/CoreText.h>
#import <CoreGraphics/CoreGraphics.h>

/* Stack XXH3_state_t / streaming helpers need full state layouts from xxhash.h */
#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"

#include "Graphics/CoreTextSystem.hpp"
#include "Graphics/TextSystemPrivate.hpp"

#include <Flux/Detail/SmallVector.hpp>
#include <Flux/Graphics/TextLayout.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <utility>
#include <vector>

namespace flux {

struct ContentHash {
  std::uint64_t hi = 0;
  std::uint64_t lo = 0;
  bool operator==(ContentHash const& o) const noexcept { return hi == o.hi && lo == o.lo; }
};

struct ContentHashHasher {
  std::size_t operator()(ContentHash const& h) const noexcept { return static_cast<std::size_t>(h.lo); }
};

struct ParagraphStyleKey {
  std::uint8_t wrap = 0;
  std::uint32_t lhQ8 = 0;
  std::uint32_t lhMulQ8 = 0;
  bool operator==(ParagraphStyleKey const& o) const noexcept = default;
};

struct ParagraphStyleKeyHash {
  std::size_t operator()(ParagraphStyleKey const& k) const noexcept {
    std::size_t h = k.wrap;
    h = h * 31 + k.lhQ8;
    h = h * 31 + k.lhMulQ8;
    return h;
  }
};

struct RunAttrKey {
  std::uint32_t fontId = 0;
  std::uint32_t sizeQ8 = 0;
  std::uint32_t rgba = 0;
  bool operator==(RunAttrKey const& o) const noexcept = default;
};

struct RunAttrKeyHash {
  std::size_t operator()(RunAttrKey const& k) const noexcept {
    std::size_t h = k.fontId;
    h = h * 31 + k.sizeQ8;
    h = h * 31 + k.rgba;
    return h;
  }
};

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

struct FontKeyView {
  std::string_view family;
  float weight = 400.f;
  bool italic = false;
};

struct FontKeyHash {
  using is_transparent = void;

  static std::size_t hashFields(std::string_view fam, float w, bool it) noexcept {
    std::size_t h = std::hash<std::string_view>{}(fam);
    h ^= std::hash<float>{}(w) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(it) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }

  std::size_t operator()(FontKey const& k) const noexcept {
    return hashFields(k.family, k.weight, k.italic);
  }
  std::size_t operator()(FontKeyView const& k) const noexcept {
    return hashFields(k.family, k.weight, k.italic);
  }
};

struct FontKeyEq {
  using is_transparent = void;
  bool operator()(FontKey const& a, FontKey const& b) const noexcept {
    return a.family == b.family && a.weight == b.weight && a.italic == b.italic;
  }
  bool operator()(FontKey const& a, FontKeyView const& b) const noexcept {
    return std::string_view(a.family) == b.family && a.weight == b.weight && a.italic == b.italic;
  }
  bool operator()(FontKeyView const& a, FontKey const& b) const noexcept {
    return a.family == std::string_view(b.family) && a.weight == b.weight && a.italic == b.italic;
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
  // Empty string is a valid NSString (not nil) but must not be passed as kCTFontFamilyNameAttribute.
  if (!fam || fam.length == 0) {
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

/// Maps a UTF-16 `NSRange` in `ns` to half-open UTF-8 byte offsets `[outBegin, outEnd)` in the UTF-8 encoding of `ns`.
static void utf16RangeToUtf8ByteRange(NSString* ns, NSRange r16, std::uint32_t& outBegin, std::uint32_t& outEnd) {
  if (!ns || [ns length] == 0) {
    outBegin = 0;
    outEnd = 0;
    return;
  }
  NSUInteger const len = [ns length];
  NSUInteger u16Start = r16.location;
  NSUInteger u16End = NSMaxRange(r16);
  if (u16Start > len) {
    u16Start = len;
  }
  if (u16End > len) {
    u16End = len;
  }
  NSString* const prefixStart = u16Start > 0 ? [ns substringToIndex:u16Start] : @"";
  NSString* const prefixEnd = u16End > 0 ? [ns substringToIndex:u16End] : @"";
  NSData* const d0 = [prefixStart dataUsingEncoding:NSUTF8StringEncoding];
  NSData* const d1 = [prefixEnd dataUsingEncoding:NSUTF8StringEncoding];
  NSUInteger const b0 = d0 ? [d0 length] : 0u;
  NSUInteger const b1 = d1 ? [d1 length] : 0u;
  outBegin = static_cast<std::uint32_t>(b0);
  outEnd = static_cast<std::uint32_t>(b1);
}

// --- ContentHash (XXH3 128-bit) -------------------------------------------------

static std::uint32_t rgba8Pack(Color const& c) {
  auto ch = [](float x) -> std::uint32_t {
    return static_cast<std::uint32_t>(std::clamp(x, 0.f, 1.f) * 255.f + 0.5f);
  };
  return (ch(c.r) << 24) | (ch(c.g) << 16) | (ch(c.b) << 8) | ch(c.a);
}

// computeContentHash / computeContentHashPlain are defined after CoreTextSystem::Impl (needs findFontId).

static std::uint32_t quantizeWidth(float maxWidth) {
  if (maxWidth <= 0.f) {
    return 0;
  }
  return static_cast<std::uint32_t>(std::lround(maxWidth * 2.f));
}

static std::uint16_t quantizeFirstBaseline(float v) {
  return static_cast<std::uint16_t>(std::clamp(std::lround(v * 8.f), 0L, 65535L));
}

static ParagraphStyleKey paragraphKeyFor(TextLayoutOptions const& opt) {
  ParagraphStyleKey k{};
  k.wrap = static_cast<std::uint8_t>(opt.wrapping);
  if (opt.lineHeightMultiple > 0.f) {
    k.lhMulQ8 = static_cast<std::uint32_t>(std::lround(opt.lineHeightMultiple * 256.f));
  } else if (opt.lineHeight > 0.f) {
    k.lhQ8 = static_cast<std::uint32_t>(std::lround(opt.lineHeight * 4.f));
  }
  return k;
}

/// Same paragraph metrics as the previous `applyParagraphStyleToMutable` (creates a new ref each time).
static CTParagraphStyleRef createParagraphStyleRef(TextLayoutOptions const& options) {
  CTLineBreakMode lineBreak = kCTLineBreakByWordWrapping;
  if (options.wrapping == TextWrapping::WrapAnywhere) {
    lineBreak = kCTLineBreakByCharWrapping;
  }

  CTParagraphStyleSetting settings[5];
  std::size_t n = 0;
  settings[n].spec = kCTParagraphStyleSpecifierLineBreakMode;
  settings[n].valueSize = sizeof(lineBreak);
  settings[n].value = &lineBreak;
  ++n;

  CGFloat minLh = 0;
  CGFloat maxLh = 0;
  CGFloat lineMultiple = 0;
  if (options.lineHeightMultiple > 0.f) {
    lineMultiple = static_cast<CGFloat>(options.lineHeightMultiple);
    settings[n].spec = kCTParagraphStyleSpecifierLineHeightMultiple;
    settings[n].valueSize = sizeof(lineMultiple);
    settings[n].value = &lineMultiple;
    ++n;
  } else if (options.lineHeight > 0.f) {
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

  return CTParagraphStyleCreate(settings, static_cast<CFIndex>(n));
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
                                     TextLayout& layout, NSString* fullString, CFIndex ctLineIndex) {
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

  CFRange const strRange = CTRunGetStringRange(run);
  NSRange const nsr = NSMakeRange(static_cast<NSUInteger>(strRange.location), static_cast<NSUInteger>(strRange.length));
  utf16RangeToUtf8ByteRange(fullString, nsr, placed.utf8Begin, placed.utf8End);
  placed.ctLineIndex = static_cast<std::uint32_t>(ctLineIndex);

  layout.runs.push_back(std::move(placed));
}

} // namespace

// --- Cache entry types (file-local) -------------------------------------------

struct MeasureSlot {
  std::uint32_t maxWidthQ1 = 0;
  std::int32_t maxLines = 0;
  Size measuredSize{};
};

struct BoxSlot {
  std::uint32_t boxWQ1 = 0;
  std::uint32_t boxHQ1 = 0;
  std::uint8_t hAlign = 0;
  std::uint8_t vAlign = 0;
  std::uint16_t firstBaselineQ8 = 0;
  std::shared_ptr<TextLayout const> layout;
};

struct LayoutSlot {
  std::uint32_t maxWidthQ1 = 0;
  std::int32_t maxLines = 0;
  std::shared_ptr<TextLayout const> unboxed;
  flux::detail::SmallVector<BoxSlot, 4> boxes;
};

struct FramesetterEntry {
  CFAttributedStringRef attrString = nullptr;
  CTFramesetterRef framesetter = nullptr;
  flux::detail::SmallVector<MeasureSlot, 4> measures;
  flux::detail::SmallVector<LayoutSlot, 4> layouts;
  flux::detail::SmallVector<std::uint32_t, 4> fontIds;
  std::uint64_t lastTouchFrame = 0;
  std::uint32_t approxBytes = 0;
};

static std::size_t countStoredTextLayouts(FramesetterEntry const& e) {
  std::size_t n = 0;
  for (auto const& ls : e.layouts) {
    if (ls.unboxed) {
      ++n;
    }
    n += ls.boxes.size();
  }
  return n;
}

static std::uint32_t estimateEntryBytes(AttributedString const& text, std::size_t layoutPieces) {
  return static_cast<std::uint32_t>(text.utf8.size() + text.runs.size() * 64 + 256 +
                                    text.utf8.size() * 32 + layoutPieces * sizeof(TextLayout));
}

struct CoreTextSystem::Impl {
  std::unordered_map<FontKey, std::uint32_t, FontKeyHash, FontKeyEq> fontIds_;
  std::vector<CTFontRef> fontById_;
  CGColorSpaceRef rgbColorSpace_ = nullptr;

  std::list<std::pair<std::uint64_t, CTFontRef>> sizedFontOrder_;
  std::unordered_map<std::uint64_t, std::list<std::pair<std::uint64_t, CTFontRef>>::iterator> sizedFontMap_;

  std::list<std::pair<std::uint32_t, CGColorRef>> colorOrder_;
  std::unordered_map<std::uint32_t, std::list<std::pair<std::uint32_t, CGColorRef>>::iterator> colorMap_;

  std::list<std::pair<RunAttrKey, CFDictionaryRef>> runAttrOrder_;
  std::unordered_map<RunAttrKey, std::list<std::pair<RunAttrKey, CFDictionaryRef>>::iterator, RunAttrKeyHash>
      runAttrMap_;

  std::list<std::pair<ParagraphStyleKey, CTParagraphStyleRef>> paraOrder_;
  std::unordered_map<ParagraphStyleKey, std::list<std::pair<ParagraphStyleKey, CTParagraphStyleRef>>::iterator,
                      ParagraphStyleKeyHash>
      paraMap_;

  std::unordered_map<ContentHash, std::unique_ptr<FramesetterEntry>, ContentHashHasher> frameMap_;
  std::size_t frameMapBytes_ = 0;
  std::uint64_t currentFrame_ = 0;
  std::size_t budgetBytes_ = 48u * 1024u * 1024u;
  TextCacheStats stats_{};

  std::optional<std::uint32_t> findFontId(std::string_view family, float weight, bool italic) const noexcept {
    FontKeyView const kv{family, weight > 0.f ? weight : kDefaultFontWeight, italic};
    auto it = fontIds_.find(kv);
    if (it != fontIds_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

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

  void bumpEntryApproxBytes(FramesetterEntry& e, AttributedString const& text);

  FramesetterEntry& insertFramesetterMiss(ContentHash const& h, AttributedString const& text,
                                          std::vector<ResolvedStyle> const& resolved,
                                          TextLayoutOptions const& options, CoreTextSystem& sys);

  FramesetterEntry& findOrInsertFramesetterEntry(ContentHash const& h, AttributedString const& text,
                                                 std::vector<ResolvedStyle> const& resolved,
                                                 TextLayoutOptions const& options, CoreTextSystem& sys);

  ContentHash computeContentHash(CoreTextSystem& sys, AttributedString const& text,
                                 std::vector<ResolvedStyle> const& resolved, TextLayoutOptions const& opt);

  ContentHash computeContentHashPlain(CoreTextSystem& sys, std::string_view utf8, Font const& font,
                                      Color const& color, TextLayoutOptions const& opt);

  void touchLruColor(std::uint32_t rgba) {
    auto it = colorMap_.find(rgba);
    if (it == colorMap_.end()) {
      return;
    }
    colorOrder_.splice(colorOrder_.begin(), colorOrder_, it->second);
  }

  CGColorRef colorRef(std::uint32_t rgba) {
    auto it = colorMap_.find(rgba);
    if (it != colorMap_.end()) {
      touchLruColor(rgba);
      ++stats_.l1_color.hits;
      return it->second->second;
    }
    ++stats_.l1_color.misses;
    constexpr std::size_t kCap = 256;
    while (colorMap_.size() >= kCap) {
      auto& last = colorOrder_.back();
      CGColorRelease(last.second);
      colorMap_.erase(last.first);
      colorOrder_.pop_back();
      ++stats_.l1_color.evictions;
    }
    float const r = static_cast<float>((rgba >> 24) & 0xFF) / 255.f;
    float const g = static_cast<float>((rgba >> 16) & 0xFF) / 255.f;
    float const b = static_cast<float>((rgba >> 8) & 0xFF) / 255.f;
    float const a = static_cast<float>(rgba & 0xFF) / 255.f;
    CGFloat comp[4] = {r, g, b, a};
    if (!rgbColorSpace_) {
      rgbColorSpace_ = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    }
    CGColorRef cg = CGColorCreate(rgbColorSpace_, comp);
    colorOrder_.push_front(std::pair<std::uint32_t, CGColorRef>{rgba, cg});
    colorMap_[rgba] = colorOrder_.begin();
    return cg;
  }

  CTFontRef sizedFont(std::uint32_t fontId, std::uint32_t sizeQ8) {
    std::uint64_t const key = (static_cast<std::uint64_t>(fontId) << 32) |
                              static_cast<std::uint64_t>(sizeQ8);
    auto it = sizedFontMap_.find(key);
    if (it != sizedFontMap_.end()) {
      sizedFontOrder_.splice(sizedFontOrder_.begin(), sizedFontOrder_, it->second);
      ++stats_.l0_sizedFont.hits;
      return it->second->second;
    }
    ++stats_.l0_sizedFont.misses;
    constexpr std::size_t kCap = 256;
    while (sizedFontMap_.size() >= kCap) {
      auto& last = sizedFontOrder_.back();
      CFRelease(last.second);
      sizedFontMap_.erase(last.first);
      sizedFontOrder_.pop_back();
      ++stats_.l0_sizedFont.evictions;
    }
    if (fontId >= fontById_.size()) {
      return nullptr;
    }
    CTFontRef base = fontById_[fontId];
    float const pt = static_cast<float>(sizeQ8) / 4.f;
    CTFontRef newf = CTFontCreateCopyWithAttributes(base, static_cast<CGFloat>(pt), nullptr, nullptr);
    sizedFontOrder_.push_front(std::pair<std::uint64_t, CTFontRef>{key, newf});
    sizedFontMap_[key] = sizedFontOrder_.begin();
    return newf;
  }

  void touchLruPara(ParagraphStyleKey const& pk) {
    auto it = paraMap_.find(pk);
    if (it == paraMap_.end()) {
      return;
    }
    paraOrder_.splice(paraOrder_.begin(), paraOrder_, it->second);
  }

  CTParagraphStyleRef paragraphStyleRef(TextLayoutOptions const& opt) {
    ParagraphStyleKey const pk = paragraphKeyFor(opt);
    auto it = paraMap_.find(pk);
    if (it != paraMap_.end()) {
      touchLruPara(pk);
      ++stats_.l1_paraStyle.hits;
      return it->second->second;
    }
    ++stats_.l1_paraStyle.misses;
    constexpr std::size_t kCap = 32;
    while (paraMap_.size() >= kCap) {
      auto& last = paraOrder_.back();
      CFRelease(last.second);
      paraMap_.erase(last.first);
      paraOrder_.pop_back();
      ++stats_.l1_paraStyle.evictions;
    }
    CTParagraphStyleRef ps = createParagraphStyleRef(opt);
    paraOrder_.push_front(std::pair<ParagraphStyleKey, CTParagraphStyleRef>{pk, ps});
    paraMap_[pk] = paraOrder_.begin();
    return ps;
  }

  void touchLruRunAttr(RunAttrKey const& k) {
    auto it = runAttrMap_.find(k);
    if (it == runAttrMap_.end()) {
      return;
    }
    runAttrOrder_.splice(runAttrOrder_.begin(), runAttrOrder_, it->second);
  }

  CFDictionaryRef runAttrDict(std::uint32_t fontId, std::uint32_t sizeQ8, std::uint32_t rgba) {
    RunAttrKey const key{fontId, sizeQ8, rgba};
    auto it = runAttrMap_.find(key);
    if (it != runAttrMap_.end()) {
      touchLruRunAttr(key);
      ++stats_.l1_runAttr.hits;
      return it->second->second;
    }
    ++stats_.l1_runAttr.misses;
    constexpr std::size_t kCap = 1024;
    while (runAttrMap_.size() >= kCap) {
      auto& last = runAttrOrder_.back();
      CFRelease(last.second);
      runAttrMap_.erase(last.first);
      runAttrOrder_.pop_back();
      ++stats_.l1_runAttr.evictions;
    }
    CTFontRef font = sizedFont(fontId, sizeQ8);
    CGColorRef cg = colorRef(rgba);
    NSDictionary* attrs = @{
      (id)kCTFontAttributeName : (__bridge id)font,
      (id)kCTForegroundColorAttributeName : (__bridge id)cg,
    };
    CFDictionaryRef stored = (__bridge_retained CFDictionaryRef)attrs;
    runAttrOrder_.push_front(std::pair<RunAttrKey, CFDictionaryRef>{key, stored});
    runAttrMap_[key] = runAttrOrder_.begin();
    return stored;
  }

  CFAttributedStringRef createCFAttributed(CoreTextSystem& sys, AttributedString const& text,
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
      std::string_view const fam =
          a.font.family.empty() ? std::string_view(kDefaultFontFamily) : std::string_view(a.font.family);
      std::uint32_t fid = 0;
      if (auto const o = findFontId(fam, a.font.weight, a.font.italic)) {
        fid = *o;
      } else {
        fid = sys.resolveFontId(fam, a.font.weight, a.font.italic);
      }
      std::uint32_t const sizeQ8 = static_cast<std::uint32_t>(std::lround(a.font.size * 4.f));
      std::uint32_t const rgba = rgba8Pack(a.color);
      CFDictionaryRef attrs = runAttrDict(fid, sizeQ8, rgba);
      [mas addAttributes:(__bridge id)attrs range:range];
    }

    CTParagraphStyleRef ps = paragraphStyleRef(options);
    NSDictionary* paraAttrs = @{ (id)kCTParagraphStyleAttributeName : (__bridge id)ps };
    [mas addAttributes:paraAttrs range:NSMakeRange(0, [mas length])];
    return (__bridge_retained CFAttributedStringRef)mas;
  }

  CFAttributedStringRef createCFAttributedPlain(CoreTextSystem& sys, std::string_view utf8, Font const& font,
                                                Color const& color, TextLayoutOptions const& options) {
    AttributedString as;
    as.utf8 = std::string(utf8);
    as.runs.push_back({0, static_cast<std::uint32_t>(utf8.size()), font, color});
    std::vector<ResolvedStyle> resolved;
    accumulateInheritance(resolved, as);
    return createCFAttributed(sys, as, resolved, options);
  }

  void releaseFramesetterEntry(FramesetterEntry& e) {
    if (e.attrString) {
      CFRelease(e.attrString);
      e.attrString = nullptr;
    }
    if (e.framesetter) {
      CFRelease(e.framesetter);
      e.framesetter = nullptr;
    }
    e.measures.clear();
    e.layouts.clear();
    e.fontIds.clear();
    e.approxBytes = 0;
  }

  ~Impl() {
    for (auto& p : frameMap_) {
      releaseFramesetterEntry(*p.second);
    }
    frameMap_.clear();
    for (CTFontRef f : fontById_) {
      if (f) {
        CFRelease(f);
      }
    }
    for (auto& kv : sizedFontOrder_) {
      CFRelease(kv.second);
    }
    sizedFontOrder_.clear();
    sizedFontMap_.clear();
    for (auto& kv : colorOrder_) {
      CGColorRelease(kv.second);
    }
    colorOrder_.clear();
    colorMap_.clear();
    for (auto& kv : runAttrOrder_) {
      CFRelease(kv.second);
    }
    runAttrOrder_.clear();
    runAttrMap_.clear();
    for (auto& kv : paraOrder_) {
      CFRelease(kv.second);
    }
    paraOrder_.clear();
    paraMap_.clear();
    if (rgbColorSpace_) {
      CGColorSpaceRelease(rgbColorSpace_);
      rgbColorSpace_ = nullptr;
    }
  }
};

void CoreTextSystem::Impl::bumpEntryApproxBytes(FramesetterEntry& e, AttributedString const& text) {
  std::size_t const pieces = countStoredTextLayouts(e);
  std::uint32_t const newApprox = estimateEntryBytes(text, pieces);
  std::uint32_t const old = e.approxBytes;
  e.approxBytes = newApprox;
  frameMapBytes_ = frameMapBytes_ - static_cast<std::size_t>(old) + static_cast<std::size_t>(newApprox);
}

FramesetterEntry& CoreTextSystem::Impl::insertFramesetterMiss(ContentHash const& h,
                                                              AttributedString const& text,
                                                              std::vector<ResolvedStyle> const& resolved,
                                                              TextLayoutOptions const& options,
                                                              CoreTextSystem& sys) {
  if (!options.suppressCacheStats) {
    ++stats_.l2_framesetter.misses;
  }
  auto entry = std::make_unique<FramesetterEntry>();
  CFAttributedStringRef cf = createCFAttributed(sys, text, resolved, options);
  entry->attrString = cf;
  CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(cf);
  entry->framesetter = fs;
  for (auto const& rs : resolved) {
    std::string_view const fam =
        rs.font.family.empty() ? std::string_view(kDefaultFontFamily) : std::string_view(rs.font.family);
    entry->fontIds.push_back(sys.resolveFontId(fam, rs.font.weight, rs.font.italic));
  }
  entry->approxBytes = estimateEntryBytes(text, 0);
  frameMapBytes_ += entry->approxBytes;
  frameMap_[h] = std::move(entry);
  return *frameMap_.find(h)->second;
}

FramesetterEntry& CoreTextSystem::Impl::findOrInsertFramesetterEntry(ContentHash const& h,
                                                                     AttributedString const& text,
                                                                     std::vector<ResolvedStyle> const& resolved,
                                                                     TextLayoutOptions const& options,
                                                                     CoreTextSystem& sys) {
  auto it = frameMap_.find(h);
  if (it != frameMap_.end()) {
    if (!options.suppressCacheStats) {
      ++stats_.l2_framesetter.hits;
    }
    it->second->lastTouchFrame = currentFrame_;
    return *it->second;
  }
  return insertFramesetterMiss(h, text, resolved, options, sys);
}

static void hashLayoutOptions(XXH3_state_t& st, TextLayoutOptions const& opt) {
  std::uint8_t wrap = static_cast<std::uint8_t>(opt.wrapping);
  std::uint32_t lhQ8 = 0;
  std::uint32_t lhMulQ8 = 0;
  if (opt.lineHeightMultiple > 0.f) {
    lhMulQ8 = static_cast<std::uint32_t>(std::lround(opt.lineHeightMultiple * 256.f));
  } else if (opt.lineHeight > 0.f) {
    lhQ8 = static_cast<std::uint32_t>(std::lround(opt.lineHeight * 4.f));
  }
  XXH3_128bits_update(&st, &wrap, sizeof(wrap));
  XXH3_128bits_update(&st, &lhQ8, sizeof(lhQ8));
  XXH3_128bits_update(&st, &lhMulQ8, sizeof(lhMulQ8));
}

ContentHash CoreTextSystem::Impl::computeContentHash(CoreTextSystem& sys, AttributedString const& text,
                                                     std::vector<ResolvedStyle> const& resolved,
                                                     TextLayoutOptions const& opt) {
  XXH3_state_t st{};
  XXH3_128bits_reset(&st);
  if (!text.utf8.empty()) {
    XXH3_128bits_update(&st, text.utf8.data(), text.utf8.size());
  }
  std::uint32_t const runCount = static_cast<std::uint32_t>(resolved.size());
  XXH3_128bits_update(&st, &runCount, sizeof(runCount));
  for (std::size_t i = 0; i < text.runs.size(); ++i) {
    auto const& run = text.runs[i];
    ResolvedStyle const& rs = resolved[i];
    std::string_view const fam =
        rs.font.family.empty() ? std::string_view(kDefaultFontFamily) : std::string_view(rs.font.family);
    std::uint32_t fid = 0;
    if (auto const o = findFontId(fam, rs.font.weight, rs.font.italic)) {
      fid = *o;
    } else {
      fid = sys.resolveFontId(fam, rs.font.weight, rs.font.italic);
    }
    std::uint32_t const sizeQ8 = static_cast<std::uint32_t>(std::lround(rs.font.size * 4.f));
    std::uint32_t const rgba = rgba8Pack(rs.color);
    std::uint32_t b0 = run.start;
    std::uint32_t b1 = run.end;
    XXH3_128bits_update(&st, &b0, sizeof(b0));
    XXH3_128bits_update(&st, &b1, sizeof(b1));
    XXH3_128bits_update(&st, &fid, sizeof(fid));
    XXH3_128bits_update(&st, &sizeQ8, sizeof(sizeQ8));
    XXH3_128bits_update(&st, &rgba, sizeof(rgba));
  }
  hashLayoutOptions(st, opt);
  XXH128_hash_t const h = XXH3_128bits_digest(&st);
  return ContentHash{h.high64, h.low64};
}

ContentHash CoreTextSystem::Impl::computeContentHashPlain(CoreTextSystem& sys, std::string_view utf8,
                                                          Font const& font, Color const& color,
                                                          TextLayoutOptions const& opt) {
  XXH3_state_t st{};
  XXH3_128bits_reset(&st);
  if (!utf8.empty()) {
    XXH3_128bits_update(&st, utf8.data(), utf8.size());
  }
  std::uint32_t const runCount = 1;
  XXH3_128bits_update(&st, &runCount, sizeof(runCount));

  Font const resolved = resolveFont(baseDefaultsFont(), font);
  std::string_view const fam =
      resolved.family.empty() ? std::string_view(kDefaultFontFamily) : std::string_view(resolved.family);
  std::uint32_t fid = 0;
  if (auto const o = findFontId(fam, resolved.weight, resolved.italic)) {
    fid = *o;
  } else {
    fid = sys.resolveFontId(fam, resolved.weight, resolved.italic);
  }
  std::uint32_t const sizeQ8 = static_cast<std::uint32_t>(std::lround(resolved.size * 4.f));
  std::uint32_t const rgba = rgba8Pack(color);
  std::uint32_t b0 = 0;
  std::uint32_t b1 = static_cast<std::uint32_t>(utf8.size());
  XXH3_128bits_update(&st, &b0, sizeof(b0));
  XXH3_128bits_update(&st, &b1, sizeof(b1));
  XXH3_128bits_update(&st, &fid, sizeof(fid));
  XXH3_128bits_update(&st, &sizeQ8, sizeof(sizeQ8));
  XXH3_128bits_update(&st, &rgba, sizeof(rgba));

  hashLayoutOptions(st, opt);
  XXH128_hash_t const h = XXH3_128bits_digest(&st);
  return ContentHash{h.high64, h.low64};
}

CoreTextSystem::CoreTextSystem() : d(std::make_unique<Impl>()) {}

CoreTextSystem::~CoreTextSystem() = default;

std::uint32_t CoreTextSystem::resolveFontId(std::string_view fontFamily, float weight, bool italic) {
  std::string_view const fam = fontFamily.empty() ? std::string_view(kDefaultFontFamily) : fontFamily;
  if (auto const o = d->findFontId(fam, weight, italic)) {
    return *o;
  }
  FontKey key;
  key.family = std::string(fam);
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

static void adjustSuggestSizeForSingleLineTrailingWhitespace(CTFramesetterRef fs, CGSize* sz) {
  if (!fs || !sz) {
    return;
  }
  CGFloat const fw = std::max(sz->width, static_cast<CGFloat>(1e-6));
  CGFloat const fh = std::max(sz->height, static_cast<CGFloat>(1e-6));
  CGPathRef path = CGPathCreateWithRect(CGRectMake(0, 0, fw, fh), nullptr);
  CTFrameRef frame = CTFramesetterCreateFrame(fs, CFRangeMake(0, 0), path, nullptr);
  CFRelease(path);
  if (!frame) {
    return;
  }
  CFArrayRef lines = CTFrameGetLines(frame);
  CFIndex const lineCount = lines ? CFArrayGetCount(lines) : 0;
  if (lineCount == 1) {
    CTLineRef const line = (CTLineRef)CFArrayGetValueAtIndex(lines, 0);
    CGFloat ascent = 0;
    CGFloat descent = 0;
    CGFloat leading = 0;
    double const tw = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
    if (tw > sz->width) {
      sz->width = static_cast<CGFloat>(tw);
    }
  }
  CFRelease(frame);
}


static Size measureWithFramesetter(CTFramesetterRef fs, float maxWidth) {
  if (!fs) {
    return {};
  }
  CGSize const constraints =
      CGSizeMake(maxWidth > 0.f ? static_cast<CGFloat>(maxWidth) : CGFLOAT_MAX, CGFLOAT_MAX);
  CFRange fitRange{};
  CGSize sz = CTFramesetterSuggestFrameSizeWithConstraints(fs, CFRangeMake(0, 0), nullptr, constraints,
                                                             &fitRange);
  adjustSuggestSizeForSingleLineTrailingWhitespace(fs, &sz);
  return Size{static_cast<float>(sz.width), static_cast<float>(sz.height)};
}

static void fillTextLayoutFromFramesetter(CoreTextSystem& sys, CTFramesetterRef fs,
                                          AttributedString const& text, float maxWidth,
                                          TextLayoutOptions const& options, TextLayout& out) {
  if (!fs) {
    return;
  }
  CGSize const constraints =
      CGSizeMake(maxWidth > 0.f ? static_cast<CGFloat>(maxWidth) : CGFLOAT_MAX, CGFLOAT_MAX);
  CFRange fitRange{};
  CGSize sz = CTFramesetterSuggestFrameSizeWithConstraints(fs, CFRangeMake(0, 0), nullptr, constraints,
                                                           &fitRange);
  adjustSuggestSizeForSingleLineTrailingWhitespace(fs, &sz);

  CGFloat const fw = std::max(static_cast<CGFloat>(sz.width), static_cast<CGFloat>(1e-6));
  CGFloat const fh = std::max(static_cast<CGFloat>(sz.height), static_cast<CGFloat>(1e-6));
  out.measuredSize = Size{static_cast<float>(fw), static_cast<float>(fh)};

  CGPathRef path = CGPathCreateWithRect(CGRectMake(0, 0, fw, fh), nullptr);
  CTFrameRef frame = CTFramesetterCreateFrame(fs, CFRangeMake(0, 0), path, nullptr);
  CFRelease(path);
  if (!frame) {
    return;
  }

  CFArrayRef lines = CTFrameGetLines(frame);
  CFIndex const lineCount = lines ? CFArrayGetCount(lines) : 0;
  std::vector<CGPoint> origins(static_cast<std::size_t>(std::max(lineCount, CFIndex{0})));
  if (lineCount > 0) {
    CTFrameGetLineOrigins(frame, CFRangeMake(0, lineCount), origins.data());
  }

  NSString* const nsFull = [NSString stringWithUTF8String:text.utf8.c_str()];

  for (CFIndex li = 0; li < lineCount; ++li) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(lines, li);
    CGPoint const lineOrigin = origins[static_cast<std::size_t>(li)];
    CFArrayRef glyphRuns = CTLineGetGlyphRuns(line);
    CFIndex const runCount = CFArrayGetCount(glyphRuns);
    for (CFIndex ri = 0; ri < runCount; ++ri) {
      CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(glyphRuns, ri);
      appendPlacedRunFromCTRun(run, lineOrigin, fh, sys, out, nsFull, li);
    }
  }

  out.lines.clear();
  out.lines.reserve(static_cast<std::size_t>(std::max(lineCount, CFIndex{0})));
  for (CFIndex li = 0; li < lineCount; ++li) {
    CTLineRef line = (CTLineRef)CFArrayGetValueAtIndex(lines, li);
    CFRange const rng = CTLineGetStringRange(line);
    NSRange const nsr = NSMakeRange(static_cast<NSUInteger>(rng.location), static_cast<NSUInteger>(rng.length));
    TextLayout::LineRange lr{};
    lr.ctLineIndex = static_cast<std::uint32_t>(li);
    std::uint32_t b0 = 0;
    std::uint32_t b1 = 0;
    utf16RangeToUtf8ByteRange(nsFull, nsr, b0, b1);
    lr.byteStart = static_cast<int>(b0);
    lr.byteEnd = static_cast<int>(b1);

    float baselineY = -std::numeric_limits<float>::infinity();
    float minTop = std::numeric_limits<float>::infinity();
    float maxBot = -std::numeric_limits<float>::infinity();
    float minOx = std::numeric_limits<float>::infinity();
    for (auto const& pr : out.runs) {
      if (pr.ctLineIndex != lr.ctLineIndex) {
        continue;
      }
      baselineY = std::max(baselineY, pr.origin.y);
      minTop = std::min(minTop, pr.origin.y - pr.run.ascent);
      maxBot = std::max(maxBot, pr.origin.y + pr.run.descent);
      minOx = std::min(minOx, pr.origin.x);
    }
    lr.baseline = baselineY;
    lr.top = minTop;
    lr.bottom = maxBot;
    lr.lineMinX = std::isfinite(minOx) ? minOx : 0.f;
    out.lines.push_back(lr);
  }

  CFRelease(frame);

  recomputeTextLayoutMetrics(out);
  if (out.measuredSize.width <= 0.f && out.measuredSize.height <= 0.f && !text.utf8.empty()) {
    out.measuredSize = Size{static_cast<float>(fw), static_cast<float>(fh)};
  }
  if (options.maxLines > 0) {
    trimTextLayoutToMaxLines(out, options.maxLines, true);
  }
}

std::vector<std::uint8_t> CoreTextSystem::rasterizeGlyph(std::uint32_t fontId, std::uint16_t glyphId,
                                                        float size, std::uint32_t& outWidth,
                                                        std::uint32_t& outHeight, Point& outBearing) {
  outWidth = 0;
  outHeight = 0;
  outBearing = {0, 0};
  std::uint32_t const sizeQ8 = static_cast<std::uint32_t>(std::lround(size * 4.f));
  CTFontRef font = d->sizedFont(fontId, sizeQ8);
  if (!font) {
    return {};
  }
  CGGlyph g = glyphId;
  CGRect bounds = CGRectZero;
  CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationHorizontal, &g, &bounds, 1);
  if (bounds.size.width <= 0 || bounds.size.height <= 0) {
    return {};
  }

  CGFloat const pad = kPadPx;
  CGFloat const ascent = CTFontGetAscent(font);
  CGFloat const descent = CTFontGetDescent(font);
  CGFloat const glyphTop = bounds.origin.y + bounds.size.height;
  double const minBoxH = static_cast<double>(pad + ascent + descent + pad);
  std::uint32_t const bw =
      static_cast<std::uint32_t>(std::ceil(static_cast<double>(bounds.size.width + pad * 2.f)));
  std::uint32_t const bh = static_cast<std::uint32_t>(
      std::max(std::ceil(static_cast<double>(bounds.size.height + pad * 2.f)), std::ceil(minBoxH)));
  if (bw == 0 || bh == 0) {
    return {};
  }

  std::vector<std::uint8_t> grayBuf(static_cast<std::size_t>(bw) * bh);
  CGColorSpaceRef grayCs = CGColorSpaceCreateDeviceGray();
  CGContextRef ctx =
      CGBitmapContextCreate(grayBuf.data(), bw, bh, 8, bw, grayCs, kCGImageAlphaNone);
  CGColorSpaceRelease(grayCs);
  if (!ctx) {
    return {};
  }

  CGContextSetShouldAntialias(ctx, true);
  CGContextSetShouldSmoothFonts(ctx, true);
  CGContextSetGrayFillColor(ctx, 1, 1);
  CGContextFillRect(ctx, CGRectMake(0, 0, bw, bh));
  CGContextSetGrayFillColor(ctx, 0, 1);

  CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
  CGFloat const ox = static_cast<CGFloat>(pad) - bounds.origin.x;
  CGFloat const baselineY = static_cast<CGFloat>(bh) - pad - glyphTop;
  CGContextSetTextPosition(ctx, ox, baselineY);
  CGPoint const z = CGPointZero;
  CTFontDrawGlyphs(font, &g, &z, 1, ctx);

  CGContextRelease(ctx);

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

Size CoreTextSystem::measure(AttributedString const& text, float maxWidth, TextLayoutOptions const& options) {
  if (text.utf8.empty()) {
    return {};
  }
  if (options.maxLines > 0) {
    std::shared_ptr<TextLayout const> const L = layout(text, maxWidth, options);
    return L ? L->measuredSize : Size{};
  }
  validateRuns(text);
  std::vector<ResolvedStyle> resolved;
  accumulateInheritance(resolved, text);
  ContentHash const h = d->computeContentHash(*this, text, resolved, options);
  std::uint32_t const wq = quantizeWidth(maxWidth);
  std::int32_t const ml = options.maxLines;

  FramesetterEntry& e = d->findOrInsertFramesetterEntry(h, text, resolved, options, *this);

  for (std::size_t i = 0; i < e.measures.size(); ++i) {
    MeasureSlot& ms = e.measures[i];
    if (ms.maxWidthQ1 == wq && ms.maxLines == ml) {
      return ms.measuredSize;
    }
  }
  Size const sz = measureWithFramesetter(e.framesetter, maxWidth);
  e.measures.push_back(MeasureSlot{wq, ml, sz});
  return sz;
}

Size CoreTextSystem::measure(std::string_view utf8, Font const& font, Color const& color, float maxWidth,
                             TextLayoutOptions const& options) {
  if (utf8.empty()) {
    return {};
  }
  if (options.maxLines > 0) {
    std::shared_ptr<TextLayout const> const L = layout(utf8, font, color, maxWidth, options);
    return L ? L->measuredSize : Size{};
  }
  ContentHash const h = d->computeContentHashPlain(*this, utf8, font, color, options);
  std::uint32_t const wq = quantizeWidth(maxWidth);
  std::int32_t const ml = options.maxLines;

  FramesetterEntry& e = [&]() -> FramesetterEntry& {
    auto it = d->frameMap_.find(h);
    if (it != d->frameMap_.end()) {
      if (!options.suppressCacheStats) {
        ++d->stats_.l2_framesetter.hits;
      }
      it->second->lastTouchFrame = d->currentFrame_;
      return *it->second;
    }
    AttributedString as;
    as.utf8 = std::string(utf8);
    as.runs.push_back({0, static_cast<std::uint32_t>(utf8.size()), font, color});
    std::vector<ResolvedStyle> resolved;
    accumulateInheritance(resolved, as);
    return d->insertFramesetterMiss(h, as, resolved, options, *this);
  }();

  for (std::size_t i = 0; i < e.measures.size(); ++i) {
    MeasureSlot& ms = e.measures[i];
    if (ms.maxWidthQ1 == wq && ms.maxLines == ml) {
      return ms.measuredSize;
    }
  }
  Size const sz = measureWithFramesetter(e.framesetter, maxWidth);
  e.measures.push_back(MeasureSlot{wq, ml, sz});
  return sz;
}

std::shared_ptr<TextLayout const> CoreTextSystem::layoutUnboxed(AttributedString const& text,
                                                                TextLayoutOptions const& options,
                                                                float maxWidth, bool hasPrecomputedHash,
                                                                std::uint64_t preHi, std::uint64_t preLo) {
  if (text.utf8.empty()) {
    return std::shared_ptr<TextLayout const>(std::make_shared<TextLayout>());
  }
  validateRuns(text);
  std::vector<ResolvedStyle> resolved;
  accumulateInheritance(resolved, text);
  ContentHash const h =
      hasPrecomputedHash ? ContentHash{preHi, preLo} : d->computeContentHash(*this, text, resolved, options);
  FramesetterEntry& e = d->findOrInsertFramesetterEntry(h, text, resolved, options, *this);
  std::uint32_t const wq = quantizeWidth(maxWidth);
  std::int32_t const ml = options.maxLines;

  for (std::size_t i = 0; i < e.layouts.size(); ++i) {
    LayoutSlot& ls = e.layouts[i];
    if (ls.maxWidthQ1 == wq && ls.maxLines == ml) {
      if (!options.suppressCacheStats) {
        ++d->stats_.l3_layout.hits;
      }
      return ls.unboxed;
    }
  }
  if (!options.suppressCacheStats) {
    ++d->stats_.l3_layout.misses;
  }

  auto built = std::make_shared<TextLayout>();
  fillTextLayoutFromFramesetter(*this, e.framesetter, text, maxWidth, options, *built);
  std::shared_ptr<TextLayout const> result = built;
  LayoutSlot slot;
  slot.maxWidthQ1 = wq;
  slot.maxLines = ml;
  slot.unboxed = result;
  e.layouts.push_back(std::move(slot));
  d->bumpEntryApproxBytes(e, text);
  return result;
}

std::shared_ptr<TextLayout const> CoreTextSystem::layout(std::string_view utf8, Font const& font,
                                                         Color const& color, float maxWidth,
                                                         TextLayoutOptions const& options) {
  if (utf8.empty()) {
    return std::shared_ptr<TextLayout const>(std::make_shared<TextLayout>());
  }
  ContentHash const h = d->computeContentHashPlain(*this, utf8, font, color, options);
  std::uint32_t const wq = quantizeWidth(maxWidth);
  std::int32_t const ml = options.maxLines;

  FramesetterEntry& e = [&]() -> FramesetterEntry& {
    auto it = d->frameMap_.find(h);
    if (it != d->frameMap_.end()) {
      if (!options.suppressCacheStats) {
        ++d->stats_.l2_framesetter.hits;
      }
      it->second->lastTouchFrame = d->currentFrame_;
      return *it->second;
    }
    AttributedString as;
    as.utf8 = std::string(utf8);
    as.runs.push_back({0, static_cast<std::uint32_t>(utf8.size()), font, color});
    std::vector<ResolvedStyle> resolved;
    accumulateInheritance(resolved, as);
    return d->insertFramesetterMiss(h, as, resolved, options, *this);
  }();

  for (std::size_t i = 0; i < e.layouts.size(); ++i) {
    LayoutSlot& ls = e.layouts[i];
    if (ls.maxWidthQ1 == wq && ls.maxLines == ml) {
      if (!options.suppressCacheStats) {
        ++d->stats_.l3_layout.hits;
      }
      return ls.unboxed;
    }
  }
  if (!options.suppressCacheStats) {
    ++d->stats_.l3_layout.misses;
  }

  AttributedString as;
  as.utf8 = std::string(utf8);
  as.runs.push_back({0, static_cast<std::uint32_t>(utf8.size()), font, color});
  auto built = std::make_shared<TextLayout>();
  fillTextLayoutFromFramesetter(*this, e.framesetter, as, maxWidth, options, *built);
  std::shared_ptr<TextLayout const> result = built;
  LayoutSlot slot;
  slot.maxWidthQ1 = wq;
  slot.maxLines = ml;
  slot.unboxed = result;
  e.layouts.push_back(std::move(slot));
  d->bumpEntryApproxBytes(e, as);
  return result;
}

std::shared_ptr<TextLayout const> CoreTextSystem::layout(AttributedString const& text, float maxWidth,
                                                         TextLayoutOptions const& options) {
  return layoutUnboxed(text, options, maxWidth, false, 0, 0);
}

std::shared_ptr<TextLayout const> CoreTextSystem::layoutBoxedImpl(AttributedString const& text, Rect const& box,
                                                                  TextLayoutOptions const& options) {
  float const maxWidth = options.wrapping == TextWrapping::NoWrap ? 0.f : box.width;
  validateRuns(text);
  std::vector<ResolvedStyle> resolved;
  accumulateInheritance(resolved, text);
  ContentHash const h = d->computeContentHash(*this, text, resolved, options);
  std::shared_ptr<TextLayout const> base = layoutUnboxed(text, options, maxWidth, true, h.hi, h.lo);
  if (!base) {
    return nullptr;
  }
  std::uint32_t const wq = quantizeWidth(maxWidth);
  std::int32_t const ml = options.maxLines;
  std::uint32_t const boxWQ = quantizeWidth(box.width);
  std::uint32_t const boxHQ = quantizeWidth(box.height);
  std::uint8_t const ha = static_cast<std::uint8_t>(options.horizontalAlignment);
  std::uint8_t const va = static_cast<std::uint8_t>(options.verticalAlignment);
  std::uint16_t const fbq = quantizeFirstBaseline(options.firstBaselineOffset);

  auto it = d->frameMap_.find(h);
  assert(it != d->frameMap_.end() && "layoutUnboxed must have inserted the framesetter entry");
  FramesetterEntry& e = *it->second;

  LayoutSlot* layoutSlot = nullptr;
  for (std::size_t li = 0; li < e.layouts.size(); ++li) {
    LayoutSlot& ls = e.layouts[li];
    if (ls.maxWidthQ1 == wq && ls.maxLines == ml) {
      layoutSlot = &ls;
      break;
    }
  }
  assert(layoutSlot && "layoutUnboxed must have populated a matching LayoutSlot");
  LayoutSlot& ls = *layoutSlot;

  for (std::size_t bi = 0; bi < ls.boxes.size(); ++bi) {
    BoxSlot& bs = ls.boxes[bi];
    if (bs.boxWQ1 == boxWQ && bs.boxHQ1 == boxHQ && bs.hAlign == ha && bs.vAlign == va &&
        bs.firstBaselineQ8 == fbq && bs.layout) {
      if (!options.suppressCacheStats) {
        ++d->stats_.l4_boxLayout.hits;
      }
      return bs.layout;
    }
  }
  if (!options.suppressCacheStats) {
    ++d->stats_.l4_boxLayout.misses;
  }
  std::shared_ptr<TextLayout> mut = cloneTextLayout(*base);
  detail::applyBoxOptions(*mut, box, options);
  auto boxed = std::shared_ptr<TextLayout const>(mut);
  ls.boxes.push_back(BoxSlot{
      .boxWQ1 = boxWQ,
      .boxHQ1 = boxHQ,
      .hAlign = ha,
      .vAlign = va,
      .firstBaselineQ8 = fbq,
      .layout = boxed,
  });
  d->bumpEntryApproxBytes(e, text);
  return boxed;
}

void CoreTextSystem::onFrameBegin(std::uint64_t frameIndex) {
  d->currentFrame_ = frameIndex;
}

void CoreTextSystem::onFrameEnd() {
  while (d->frameMapBytes_ > d->budgetBytes_) {
    std::vector<std::pair<std::uint64_t, ContentHash>> candidates;
    candidates.reserve(d->frameMap_.size());
    for (auto const& p : d->frameMap_) {
      if (p.second->lastTouchFrame >= d->currentFrame_) {
        continue;
      }
      candidates.push_back({p.second->lastTouchFrame, p.first});
    }
    if (candidates.empty()) {
      break;
    }
    std::sort(candidates.begin(), candidates.end(),
              [](auto const& a, auto const& b) { return a.first < b.first; });
    for (auto const& c : candidates) {
      if (d->frameMapBytes_ <= d->budgetBytes_) {
        break;
      }
      auto it = d->frameMap_.find(c.second);
      if (it == d->frameMap_.end()) {
        continue;
      }
      if (it->second->lastTouchFrame >= d->currentFrame_) {
        continue;
      }
      d->frameMapBytes_ -= it->second->approxBytes;
      d->releaseFramesetterEntry(*it->second);
      d->frameMap_.erase(it);
      ++d->stats_.l2_framesetter.evictions;
    }
  }
  d->stats_.l2_framesetter.currentBytes = d->frameMapBytes_;
  if (d->frameMapBytes_ > d->stats_.l2_framesetter.peakBytes) {
    d->stats_.l2_framesetter.peakBytes = d->frameMapBytes_;
  }
}

void CoreTextSystem::invalidateAll() {
  for (auto& p : d->frameMap_) {
    d->releaseFramesetterEntry(*p.second);
  }
  d->frameMap_.clear();
  d->frameMapBytes_ = 0;
}

void CoreTextSystem::invalidateForFontChange(std::span<std::uint32_t const> fontIds) {
  std::unordered_set<std::uint32_t> idset(fontIds.begin(), fontIds.end());
  std::vector<ContentHash> toErase;
  for (auto const& p : d->frameMap_) {
    for (std::uint32_t fid : p.second->fontIds) {
      if (idset.count(fid)) {
        toErase.push_back(p.first);
        break;
      }
    }
  }
  for (ContentHash const& h : toErase) {
    auto it = d->frameMap_.find(h);
    if (it != d->frameMap_.end()) {
      d->frameMapBytes_ -= it->second->approxBytes;
      d->releaseFramesetterEntry(*it->second);
      d->frameMap_.erase(it);
    }
  }
}

TextCacheStats CoreTextSystem::stats() const {
  return d->stats_;
}

} // namespace flux
