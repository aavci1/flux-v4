#include <doctest/doctest.h>

#if defined(__APPLE__)

#include "Graphics/CoreTextSystem.hpp"
#include "Graphics/TextSystemPrivate.hpp"

#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/TextCacheStats.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>

using namespace flux;

TEST_CASE("Paragraph cache: fast path matches slow path (env toggle)") {
    std::string body;
    for (int i = 0; i < 6; ++i) {
        body += std::string(100, static_cast<char>('a' + (i % 26))) + "\n";
    }
    Font f {};
    f.family = ".AppleSystemUIFont";
    f.size = 14.f;
    f.weight = 400.f;
    AttributedString const as = AttributedString::plain(body, f, Colors::black);
    TextLayoutOptions opt {};
    opt.wrapping = TextWrapping::Wrap;

    setenv("FLUX_DISABLE_PARAGRAPH_CACHE", "1", 1);
    CoreTextSystem sysSlow;
    auto const slow = sysSlow.layout(as, 420.f, opt);
    unsetenv("FLUX_DISABLE_PARAGRAPH_CACHE");

    CoreTextSystem sysFast;
    auto const fast = sysFast.layout(as, 420.f, opt);
    REQUIRE(slow != nullptr);
    REQUIRE(fast != nullptr);
    // Structural parity: same topology and near-identical bounds (per-paragraph frames may differ sub-pixel).
    CHECK(slow->runs.size() == fast->runs.size());
    CHECK(slow->lines.size() == fast->lines.size());
    CHECK(std::fabs(slow->measuredSize.width - fast->measuredSize.width) < 8.f);
    CHECK(std::fabs(slow->measuredSize.height - fast->measuredSize.height) < 8.f);
}

TEST_CASE("Paragraph cache: fast path matches slow path with run backgrounds") {
    std::string body;
    for (int i = 0; i < 6; ++i) {
        body += std::string(80, static_cast<char>('a' + (i % 26))) + "\n";
    }
    Font f {};
    f.family = ".AppleSystemUIFont";
    f.size = 14.f;
    f.weight = 400.f;

    AttributedString as;
    as.utf8 = body;
    std::uint32_t const split = static_cast<std::uint32_t>(body.size() / 2);
    as.runs.push_back({0, split, f, Colors::black, Color::hex(0xFFF59D)});
    as.runs.push_back({split, static_cast<std::uint32_t>(body.size()), f, Colors::black, std::nullopt});

    TextLayoutOptions opt {};
    opt.wrapping = TextWrapping::Wrap;

    setenv("FLUX_DISABLE_PARAGRAPH_CACHE", "1", 1);
    CoreTextSystem sysSlow;
    auto const slow = sysSlow.layout(as, 420.f, opt);
    unsetenv("FLUX_DISABLE_PARAGRAPH_CACHE");

    CoreTextSystem sysFast;
    auto const fast = sysFast.layout(as, 420.f, opt);
    REQUIRE(slow != nullptr);
    REQUIRE(fast != nullptr);
    CHECK(detail::paragraphCacheLayoutsStructurallyEqual(*slow, *fast));
}

TEST_CASE("Paragraph cache: stats layers exist after layout") {
    CoreTextSystem sys;
    std::string body;
    for (int i = 0; i < 6; ++i) {
        body += std::string(100, 'x') + "\n";
    }
    Font f {};
    f.family = ".AppleSystemUIFont";
    f.size = 14.f;
    AttributedString const as = AttributedString::plain(body, f, Colors::black);
    (void)sys.layout(as, 300.f, TextLayoutOptions {});
    TextCacheStats const st = sys.stats();
    CHECK(st.l2_5_paragraph.misses + st.l2_5_paragraph.hits >= 1);
}

TEST_CASE("Paragraph cache: variant refs survive per-paragraph LRU eviction") {
    std::string body;
    for (int i = 0; i < 6; ++i) {
        body += std::string(100, static_cast<char>('a' + (i % 26))) + "\n";
    }
    Font f {};
    f.family = ".AppleSystemUIFont";
    f.size = 14.f;
    f.weight = 400.f;
    AttributedString const as = AttributedString::plain(body, f, Colors::black);
    TextLayoutOptions opt {};
    opt.wrapping = TextWrapping::Wrap;

    float const wA = 170.f;
    CoreTextSystem sys;
    auto const held = sys.layout(as, wA, opt);
    REQUIRE(held != nullptr);
    auto const snapshot = cloneTextLayout(*held);

    (void)sys.layout(as, 410.f, opt);
    (void)sys.layout(as, 540.f, opt);

    CHECK(detail::paragraphCacheLayoutsStructurallyEqual(*held, *snapshot));
}

TEST_CASE("Paragraph cache: notdefGlyphFilteringMatchesCoreText") {
    Font f {};
    f.family = ".AppleSystemUIFont";
    f.size = 15.f;
    // Emoji + ASCII: Core Text may emit leading `.notdef` (gid 0) in CTRun glyph arrays; storage must filter.
    std::string const txt = "Hello \xF0\x9F\x98\x80 world\n";
    AttributedString const as = AttributedString::plain(txt, f, Colors::black);
    TextLayoutOptions opt {};
    opt.wrapping = TextWrapping::Wrap;
    CoreTextSystem sys;
    auto const ly = sys.layout(as, 400.f, opt);
    REQUIRE(ly != nullptr);
    REQUIRE(ly->measuredSize.width > 0.f);
    for (auto const &pr : ly->runs) {
        for (std::uint16_t g : pr.run.glyphIds) {
            CHECK(g != 0);
        }
    }
}

TEST_CASE("CoreText layout preserves attributed run backgrounds") {
    Font f {};
    f.family = ".AppleSystemUIFont";
    f.size = 15.f;

    AttributedString as;
    as.utf8 = "hello world";
    as.runs.push_back({0, 5, f, Colors::black, Color::hex(0xFFE082)});
    as.runs.push_back({5, 11, f, Colors::black, std::nullopt});

    CoreTextSystem sys;
    auto const ly = sys.layout(as, 400.f, TextLayoutOptions {});
    REQUIRE(ly != nullptr);
    REQUIRE(!ly->runs.empty());

    bool sawBackground = false;
    bool sawPlain = false;
    for (auto const &pr : ly->runs) {
        if (pr.utf8Begin < 5) {
            CHECK(pr.run.backgroundColor.has_value());
            if (pr.run.backgroundColor.has_value()) {
                CHECK(*pr.run.backgroundColor == Color::hex(0xFFE082));
            }
            sawBackground = true;
        } else {
            CHECK(!pr.run.backgroundColor.has_value());
            sawPlain = true;
        }
    }
    CHECK(sawBackground);
    CHECK(sawPlain);
}

#else

TEST_CASE("Paragraph cache tests skipped on non-Apple builds") { CHECK(true); }

#endif
