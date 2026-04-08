// Micro-benchmarks for paragraph shape cache (macOS + Core Text).
// Build: cmake -B build -DFLUX_BUILD_BENCHMARKS=ON && cmake --build build --target paragraph_cache_bench

#include "Graphics/CoreTextSystem.hpp"

#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <chrono>
#include <iostream>
#include <string>

namespace {

constexpr int kParas = 5000;
constexpr int kCharsPerPara = 80;

static std::string makeDocument() {
  std::string s;
  s.reserve(static_cast<std::size_t>(kParas) * (kCharsPerPara + 1));
  for (int i = 0; i < kParas; ++i) {
    s.append(static_cast<std::size_t>(kCharsPerPara), static_cast<char>('a' + (i % 26)));
    s.push_back('\n');
  }
  return s;
}

static double secondsSince(std::chrono::steady_clock::time_point t0) {
  auto const t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(t1 - t0).count();
}

} // namespace

int main() {
#if !defined(__APPLE__)
  std::cerr << "paragraph_cache_bench is only supported on Apple platforms.\n";
  return 0;
#else
  using namespace flux;
  std::string const doc = makeDocument();
  Font f{};
  f.family = ".AppleSystemUIFont";
  f.size = 13.f;
  f.weight = 400.f;
  AttributedString const as = AttributedString::plain(doc, f, Colors::black);
  TextLayoutOptions opt{};

  // B1 baseline: full-document framesetter path (disable paragraph cache).
  double tFull = 0;
  {
    flux::CoreTextSystem sys;
    setenv("FLUX_DISABLE_PARAGRAPH_CACHE", "1", 1);
    auto const t0 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    tFull = secondsSince(t0);
    unsetenv("FLUX_DISABLE_PARAGRAPH_CACHE");
    std::cout << "B1 T_full (slow path): " << tFull << " s\n";
  }

  // B2: one framesetter for whole document vs sum of per-paragraph typesetter builds (approximate isolation).
  {
    flux::CoreTextSystem sys;
    setenv("FLUX_DISABLE_PARAGRAPH_CACHE", "1", 1);
    auto const t0 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    double const tDoc = secondsSince(t0);
    unsetenv("FLUX_DISABLE_PARAGRAPH_CACHE");
    double sum = 0;
    std::size_t off = 0;
    while (off < doc.size()) {
      std::size_t const nl = doc.find('\n', off);
      std::size_t const end = (nl == std::string::npos) ? doc.size() : nl;
      std::string const slice = doc.substr(off, end - off);
      AttributedString one = AttributedString::plain(slice, f, Colors::black);
      auto const t1 = std::chrono::steady_clock::now();
      (void)sys.layout(one, 800.f, opt);
      sum += secondsSince(t1);
      if (nl == std::string::npos) {
        break;
      }
      off = nl + 1;
    }
    std::cout << "B2 T_doc (one framesetter pass): " << tDoc << " s\n";
    std::cout << "B2 T_sum (5000 layout() calls per paragraph, slow path): " << sum << " s\n";
  }

  // B3: paragraph-cache layout: cold vs warm assembly.
  {
    flux::CoreTextSystem sys;
    auto const t0 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    double const tCold = secondsSince(t0);
    auto const t0b = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    double const tWarm = secondsSince(t0b);
    std::cout << "B3 T_assembly cold: " << tCold << " s, warm: " << tWarm << " s\n";
  }

  // B4: per-keystroke latency on a warm cache.
  {
    flux::CoreTextSystem sys;
    (void)sys.layout(as, 800.f, opt);  // warm the cache

    // Mutate: append 'x' to the middle paragraph.
    std::string edited = doc;
    std::size_t const mid = kParas / 2;
    std::size_t const insertAt = mid * (kCharsPerPara + 1) + 40;  // middle of paragraph 2500
    edited.insert(insertAt, 1, 'x');
    AttributedString const asEdited = AttributedString::plain(edited, f, Colors::black);

    auto const t0 = std::chrono::steady_clock::now();
    (void)sys.layout(asEdited, 800.f, opt);
    double const tEdit = secondsSince(t0);
    std::cout << "B4 T_keystroke (1 paragraph changed, 4999 cached): " << tEdit << " s\n";
  }

  // B5: resize — all variants rebuild on first resize, then hit cache on second.
  {
    flux::CoreTextSystem sys;
    (void)sys.layout(as, 800.f, opt);  // prime paragraph cache at 800px

    // First resize to 600px: all paragraph variants must rebuild (cache miss)
    auto const t0 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 600.f, opt);
    double const tResize = secondsSince(t0);
    std::cout << "B5 T_resize_cold (all variants rebuild, 600px): " << tResize << " s\n";

    // Second layout at 600px: all paragraph variants hit the cache
    auto const t1 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 600.f, opt);
    double const tResizeWarm = secondsSince(t1);
    std::cout << "B5 T_resize_warm (variant cache hit, 600px): " << tResizeWarm << " s\n";

    // Back to 800px: variant cache should still have 800px variants (kMaxVariantsPerParagraph=2)
    auto const t2 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    double const tBack = secondsSince(t2);
    std::cout << "B5 T_resize_back (variant cache hit, 800px): " << tBack << " s\n";

    // Repeat exact call: memo hit
    auto const t3 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    double const tMemo = secondsSince(t3);
    std::cout << "B5 T_memo_hit (exact repeat, should be <0.1ms): " << (tMemo * 1000.0) << " ms\n";
  }

  return 0;
#endif
}
