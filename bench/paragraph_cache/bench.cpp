// Micro-benchmarks for paragraph shape cache.
// Build: cmake -B build -DFLUX_BUILD_BENCHMARKS=ON && cmake --build build --target paragraph_cache_bench

#include "Graphics/CoreTextSystem.hpp"

#include <Flux/Graphics/AttributedString.hpp>
#include <Flux/Graphics/TextLayoutOptions.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr int kParas = 5000;
constexpr int kCharsPerPara = 80;

std::string makeDocument() {
  std::string s;
  s.reserve(static_cast<std::size_t>(kParas) * (kCharsPerPara + 1));
  for (int i = 0; i < kParas; ++i) {
    s.append(static_cast<std::size_t>(kCharsPerPara), static_cast<char>('a' + (i % 26)));
    s.push_back('\n');
  }
  return s;
}

std::string makeSmallLabel(int i) {
  return "Hello World " + std::to_string(i);
}

double secondsSince(std::chrono::steady_clock::time_point t0) {
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
  {
    CoreTextSystem sys;
    setenv("FLUX_DISABLE_PARAGRAPH_CACHE", "1", 1);
    auto const t0 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    double const tFull = secondsSince(t0);
    unsetenv("FLUX_DISABLE_PARAGRAPH_CACHE");
    std::cout << "B1 T_full (slow path): " << tFull << " s\n";
  }

  // B2: one framesetter for whole document vs sum of per-paragraph typesetter builds.
  {
    CoreTextSystem sys;
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
    CoreTextSystem sys;
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
    CoreTextSystem sys;
    (void)sys.layout(as, 800.f, opt);

    std::string edited = doc;
    std::size_t const mid = kParas / 2;
    std::size_t const insertAt = mid * (kCharsPerPara + 1) + 40;
    edited.insert(insertAt, 1, 'x');
    AttributedString const asEdited = AttributedString::plain(edited, f, Colors::black);

    auto const t0 = std::chrono::steady_clock::now();
    (void)sys.layout(asEdited, 800.f, opt);
    double const tEdit = secondsSince(t0);
    std::cout << "B4 T_keystroke (1 paragraph changed, 4999 cached): " << tEdit << " s\n";
  }

  // B5: resize: first new width rebuilds variants, repeated widths hit cache.
  {
    CoreTextSystem sys;
    (void)sys.layout(as, 800.f, opt);

    auto const t0 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 600.f, opt);
    double const tResize = secondsSince(t0);
    std::cout << "B5 T_resize_cold (all variants rebuild, 600px): " << tResize << " s\n";

    auto const t1 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 600.f, opt);
    double const tResizeWarm = secondsSince(t1);
    std::cout << "B5 T_resize_warm (variant cache hit, 600px): " << tResizeWarm << " s\n";

    auto const t2 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    double const tBack = secondsSince(t2);
    std::cout << "B5 T_resize_back (variant cache hit, 800px): " << tBack << " s\n";

    auto const t3 = std::chrono::steady_clock::now();
    (void)sys.layout(as, 800.f, opt);
    double const tMemo = secondsSince(t3);
    std::cout << "B5 T_memo_hit (exact repeat, should be <0.1ms): " << (tMemo * 1000.0) << " ms\n";
  }

  // B6: Tier 0 pointer-identity hit on a stable buffer.
  {
    CoreTextSystem sys;
    (void)sys.layout(as, 800.f, opt);

    constexpr int N = 1000;
    auto const t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) {
      (void)sys.layout(as, 800.f, opt);
    }
    double const tPer = secondsSince(t0) / N;
    std::cout << "B6 T_tier0_hit (pointer-identity repeat): " << (tPer * 1e9) << " ns\n";
  }

  // B7: small-text cold plain-overload path with unique labels.
  {
    CoreTextSystem sys;
    constexpr int N = 1000;
    std::vector<std::string> labels;
    labels.reserve(N);
    for (int i = 0; i < N; ++i) {
      labels.push_back(makeSmallLabel(i));
    }

    auto const t0 = std::chrono::steady_clock::now();
    for (auto const& label : labels) {
      (void)sys.layout(std::string_view(label), f, Colors::black, 300.f, opt);
    }
    double const tPer = secondsSince(t0) / N;
    std::cout << "B7 T_small_cold (plain overload, unique labels): " << (tPer * 1e6) << " us\n";
  }

  // B8: same content with fresh owning strings; bypasses Tier 0 but hits content/layout caches.
  {
    CoreTextSystem sys;
    constexpr int N = 1000;
    std::string const label = "Hello World";
    std::vector<std::string> owners(N, label);
    (void)sys.layout(std::string_view(label), f, Colors::black, 300.f, opt);

    auto const t0 = std::chrono::steady_clock::now();
    for (auto const& owner : owners) {
      (void)sys.layout(std::string_view(owner), f, Colors::black, 300.f, opt);
    }
    double const tPer = secondsSince(t0) / N;
    std::cout << "B8 T_small_warm (plain overload, fresh owner): " << (tPer * 1e6) << " us\n";
  }

  return 0;
#endif
}
