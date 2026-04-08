#include <doctest/doctest.h>

#if defined(__APPLE__)

#include "Graphics/CoreTextSystem.hpp"

#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/TextCacheStats.hpp>
#include <Flux/Graphics/TextLayout.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <cstdlib>
#include <cmath>
#include <string>

using namespace flux;

TEST_CASE("Paragraph cache: fast path matches slow path (env toggle)") {
  std::string body;
  for (int i = 0; i < 6; ++i) {
    body += std::string(100, static_cast<char>('a' + (i % 26))) + "\n";
  }
  Font f{};
  f.family = ".AppleSystemUIFont";
  f.size = 14.f;
  f.weight = 400.f;
  AttributedString const as = AttributedString::plain(body, f, Colors::black);
  TextLayoutOptions opt{};
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

TEST_CASE("Paragraph cache: stats layers exist after layout") {
  CoreTextSystem sys;
  std::string body;
  for (int i = 0; i < 6; ++i) {
    body += std::string(100, 'x') + "\n";
  }
  Font f{};
  f.family = ".AppleSystemUIFont";
  f.size = 14.f;
  AttributedString const as = AttributedString::plain(body, f, Colors::black);
  (void)sys.layout(as, 300.f, TextLayoutOptions{});
  TextCacheStats const st = sys.stats();
  CHECK(st.l2_5_paragraph.misses + st.l2_5_paragraph.hits >= 1);
}

#else

TEST_CASE("Paragraph cache tests skipped on non-Apple builds") { CHECK(true); }

#endif
