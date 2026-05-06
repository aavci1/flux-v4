#include "Graphics/Linux/FreeTypeTextSystem.hpp"

#include <Flux/Graphics/AttributedString.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace flux {

namespace {

struct Codepoint {
  char32_t value = 0;
  std::uint32_t byteBegin = 0;
  std::uint32_t byteEnd = 0;
};

std::vector<Codepoint> decodeUtf8(std::string_view text) {
  std::vector<Codepoint> out;
  for (std::uint32_t i = 0; i < text.size();) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    char32_t cp = c;
    std::uint32_t len = 1;
    if ((c & 0xE0u) == 0xC0u && i + 1 < text.size()) {
      cp = ((c & 0x1Fu) << 6u) | (static_cast<unsigned char>(text[i + 1]) & 0x3Fu);
      len = 2;
    } else if ((c & 0xF0u) == 0xE0u && i + 2 < text.size()) {
      cp = ((c & 0x0Fu) << 12u) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6u) |
           (static_cast<unsigned char>(text[i + 2]) & 0x3Fu);
      len = 3;
    } else if ((c & 0xF8u) == 0xF0u && i + 3 < text.size()) {
      cp = ((c & 0x07u) << 18u) | ((static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 12u) |
           ((static_cast<unsigned char>(text[i + 2]) & 0x3Fu) << 6u) |
           (static_cast<unsigned char>(text[i + 3]) & 0x3Fu);
      len = 4;
    }
    out.push_back(Codepoint{cp, i, static_cast<std::uint32_t>(std::min<std::size_t>(text.size(), i + len))});
    i += len;
  }
  return out;
}

Font resolvedFont(Font f) {
  if (f.size <= 0.f) {
    f.size = 16.f;
  }
  if (f.weight <= 0.f) {
    f.weight = 400.f;
  }
  return f;
}

std::string findFontPath(std::string_view family) {
  if (family == "Material Symbols Rounded") {
    for (std::filesystem::path const& p : {
             std::filesystem::current_path() / "fonts/MaterialSymbolsRounded.ttf",
             std::filesystem::current_path() / "../fonts/MaterialSymbolsRounded.ttf",
         }) {
      if (std::filesystem::exists(p)) {
        return p.string();
      }
    }
  }
  for (char const* p : {
           "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
           "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
           "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
       }) {
    if (std::filesystem::exists(p)) {
      return p;
    }
  }
  return {};
}

} // namespace

struct FreeTypeTextSystem::Impl {
  FT_Library library = nullptr;
  std::vector<FT_Face> faces;
  std::unordered_map<std::string, std::uint32_t> ids;

  Impl() {
    if (FT_Init_FreeType(&library) != 0) {
      throw std::runtime_error("FreeType initialization failed");
    }
  }

  ~Impl() {
    for (FT_Face face : faces) {
      FT_Done_Face(face);
    }
    if (library) {
      FT_Done_FreeType(library);
    }
  }

  FT_Face face(std::uint32_t id) const {
    if (id >= faces.size()) {
      return faces.empty() ? nullptr : faces.front();
    }
    return faces[id];
  }
};

FreeTypeTextSystem::FreeTypeTextSystem() : d(std::make_unique<Impl>()) {}
FreeTypeTextSystem::~FreeTypeTextSystem() = default;

std::uint32_t FreeTypeTextSystem::resolveFontId(std::string_view fontFamily, float weight, bool italic) {
  std::string const family = fontFamily.empty() ? "sans" : std::string(fontFamily);
  std::string const key = family + ":" + std::to_string(static_cast<int>(std::lround(weight))) +
                          (italic ? ":i" : ":r");
  if (auto it = d->ids.find(key); it != d->ids.end()) {
    return it->second;
  }
  std::string path = findFontPath(family);
  if (path.empty()) {
    throw std::runtime_error("No usable Linux font found");
  }
  FT_Face face = nullptr;
  if (FT_New_Face(d->library, path.c_str(), 0, &face) != 0) {
    throw std::runtime_error("Failed to load font: " + path);
  }
  std::uint32_t const id = static_cast<std::uint32_t>(d->faces.size());
  d->faces.push_back(face);
  d->ids[key] = id;
  return id;
}

std::shared_ptr<TextLayout const> FreeTypeTextSystem::layout(std::string_view utf8, Font const& font,
                                                             Color const& color, float maxWidth,
                                                             TextLayoutOptions const& options) {
  AttributedString as = AttributedString::plain(utf8, font, color);
  return layout(as, maxWidth, options);
}

std::shared_ptr<TextLayout const> FreeTypeTextSystem::layout(AttributedString const& text, float maxWidth,
                                                             TextLayoutOptions const& options) {
  auto result = std::make_shared<TextLayout>();
  result->ownedStorage = std::make_unique<TextLayoutStorage>();
  if (text.utf8.empty()) {
    return result;
  }

  AttributedRun run = text.runs.empty()
                          ? AttributedRun{0, static_cast<std::uint32_t>(text.utf8.size()),
                                          Font::body(), Colors::black}
                          : text.runs.front();
  Font font = resolvedFont(run.font);
  std::uint32_t fontId = resolveFontId(font.family, font.weight, font.italic);
  FT_Face face = d->face(fontId);
  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::max(1.f, font.size)));

  float const ascent = static_cast<float>(face->size->metrics.ascender >> 6);
  float const descent = static_cast<float>(-(face->size->metrics.descender >> 6));
  float lineHeight = static_cast<float>((face->size->metrics.height >> 6) > 0 ? (face->size->metrics.height >> 6)
                                                                              : std::lround(font.size * 1.25f));
  if (options.lineHeight > 0.f) {
    lineHeight = std::max(lineHeight, options.lineHeight);
  } else if (options.lineHeightMultiple > 0.f) {
    lineHeight = std::max(lineHeight, static_cast<float>(std::lround(font.size * options.lineHeightMultiple)));
  }

  float x = 0.f;
  float y = ascent;
  float maxLineWidth = 0.f;
  std::uint32_t lineStartByte = 0;
  std::uint32_t lineIndex = 0;
  std::size_t lineGlyphStart = 0;
  std::size_t linePositionStart = 0;
  bool const allowWrap = options.wrapping != TextWrapping::NoWrap && maxWidth > 0.f;

  auto flushLine = [&](std::uint32_t byteEnd) {
    std::size_t const glyphCount = result->ownedStorage->glyphArena.size() - lineGlyphStart;
    if (glyphCount > 0) {
      TextLayout::PlacedRun placed{};
      placed.run.fontId = fontId;
      placed.run.fontSize = font.size;
      placed.run.color = run.color;
      placed.run.glyphIds = std::span<std::uint16_t const>(result->ownedStorage->glyphArena.data() + lineGlyphStart,
                                                           glyphCount);
      placed.run.positions = std::span<Point const>(result->ownedStorage->positionArena.data() + linePositionStart,
                                                    glyphCount);
      placed.run.ascent = ascent;
      placed.run.descent = descent;
      placed.run.width = x;
      placed.origin = {0.f, y};
      placed.utf8Begin = lineStartByte;
      placed.utf8End = byteEnd;
      placed.ctLineIndex = lineIndex;
      result->runs.push_back(placed);
    }
    result->lines.push_back(TextLayout::LineRange{lineIndex, static_cast<int>(lineStartByte),
                                                  static_cast<int>(byteEnd), 0.f, y - ascent, y + descent, y});
    maxLineWidth = std::max(maxLineWidth, x);
    ++lineIndex;
    x = 0.f;
    y += lineHeight;
    lineStartByte = byteEnd;
    lineGlyphStart = result->ownedStorage->glyphArena.size();
    linePositionStart = result->ownedStorage->positionArena.size();
  };

  std::vector<Codepoint> const codepoints = decodeUtf8(text.utf8);
  result->ownedStorage->glyphArena.reserve(codepoints.size());
  result->ownedStorage->positionArena.reserve(codepoints.size());

  for (Codepoint cp : codepoints) {
    if (cp.value == U'\n') {
      flushLine(cp.byteBegin);
      lineStartByte = cp.byteEnd;
      continue;
    }
    FT_UInt glyph = FT_Get_Char_Index(face, static_cast<FT_ULong>(cp.value));
    if (FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT) != 0) {
      continue;
    }
    float advance = static_cast<float>(face->glyph->advance.x >> 6);
    if (allowWrap && x > 0.f && x + advance > maxWidth) {
      std::uint32_t const nextLineStart = cp.value == U' ' ? cp.byteEnd : cp.byteBegin;
      flushLine(nextLineStart);
      lineStartByte = nextLineStart;
      if (cp.value == U' ') {
        continue;
      }
    }
    if (options.maxLines > 0 && static_cast<int>(lineIndex) >= options.maxLines) {
      continue;
    }
    result->ownedStorage->glyphArena.push_back(static_cast<std::uint16_t>(glyph));
    result->ownedStorage->positionArena.push_back(Point{x, 0.f});
    x += advance;
    maxLineWidth = std::max(maxLineWidth, x);
  }

  flushLine(static_cast<std::uint32_t>(text.utf8.size()));
  result->measuredSize = {maxLineWidth, result->lines.empty() ? 0.f : result->lines.back().bottom};
  result->firstBaseline = result->runs.empty() ? 0.f : result->runs.front().origin.y;
  result->lastBaseline = result->runs.empty() ? 0.f : result->runs.back().origin.y;
  return result;
}

std::shared_ptr<TextLayout const> FreeTypeTextSystem::layoutBoxedImpl(AttributedString const& text, Rect const& box,
                                                                      TextLayoutOptions const& options) {
  auto layoutResult = std::const_pointer_cast<TextLayout>(layout(text, box.width, options));
  float const contentHeight = layoutResult->measuredSize.height;
  float dy = 0.f;
  switch (options.verticalAlignment) {
  case VerticalAlignment::Top:
    break;
  case VerticalAlignment::Center:
    dy = (box.height - contentHeight) * 0.5f;
    break;
  case VerticalAlignment::Bottom:
    dy = box.height - contentHeight;
    break;
  case VerticalAlignment::FirstBaseline:
    dy = options.firstBaselineOffset - layoutResult->firstBaseline;
    break;
  }

  for (auto& run : layoutResult->runs) {
    float dx = 0.f;
    if (options.horizontalAlignment == HorizontalAlignment::Center) {
      dx = (box.width - run.run.width) * 0.5f;
    } else if (options.horizontalAlignment == HorizontalAlignment::Trailing) {
      dx = box.width - run.run.width;
    }
    run.origin.x += box.x;
    run.origin.x += dx;
    run.origin.y += box.y + dy;
  }
  for (auto& line : layoutResult->lines) {
    float lineWidth = 0.f;
    for (auto const& run : layoutResult->runs) {
      if (run.ctLineIndex == line.ctLineIndex) {
        lineWidth = std::max(lineWidth, run.run.width);
      }
    }
    float dx = 0.f;
    if (options.horizontalAlignment == HorizontalAlignment::Center) {
      dx = (box.width - lineWidth) * 0.5f;
    } else if (options.horizontalAlignment == HorizontalAlignment::Trailing) {
      dx = box.width - lineWidth;
    }
    line.lineMinX += box.x + dx;
    line.top += box.y + dy;
    line.bottom += box.y + dy;
    line.baseline += box.y + dy;
  }
  layoutResult->measuredSize.width = std::min(layoutResult->measuredSize.width, box.width);
  layoutResult->measuredSize.height = std::min(layoutResult->measuredSize.height, box.height);
  return layoutResult;
}

Size FreeTypeTextSystem::measure(std::string_view utf8, Font const& font, Color const& color,
                                 float maxWidth, TextLayoutOptions const& options) {
  return layout(utf8, font, color, maxWidth, options)->measuredSize;
}

Size FreeTypeTextSystem::measure(AttributedString const& text, float maxWidth, TextLayoutOptions const& options) {
  return layout(text, maxWidth, options)->measuredSize;
}

std::vector<std::uint8_t> FreeTypeTextSystem::rasterizeGlyph(std::uint32_t fontId, std::uint16_t glyphId,
                                                             float size, std::uint32_t& outWidth,
                                                             std::uint32_t& outHeight, Point& outBearing) {
  FT_Face face = d->face(fontId);
  if (!face) {
    outWidth = outHeight = 0;
    return {};
  }
  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(std::max(1.f, size)));
  if (FT_Load_Glyph(face, glyphId, FT_LOAD_RENDER) != 0) {
    outWidth = outHeight = 0;
    return {};
  }
  FT_GlyphSlot slot = face->glyph;
  outWidth = slot->bitmap.width;
  outHeight = slot->bitmap.rows;
  outBearing = {static_cast<float>(slot->bitmap_left), static_cast<float>(slot->bitmap_top)};
  std::vector<std::uint8_t> out(outWidth * outHeight);
  for (std::uint32_t y = 0; y < outHeight; ++y) {
    std::memcpy(out.data() + y * outWidth, slot->bitmap.buffer + y * slot->bitmap.pitch, outWidth);
  }
  return out;
}

} // namespace flux
