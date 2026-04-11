#include <doctest/doctest.h>

#include <Flux/UI/Views/TextEditUtils.hpp>

#include <Flux/Graphics/TextSystem.hpp>

#include <string>

using namespace flux;
using namespace flux::detail;

TEST_CASE("TextEditUtils: utf8 navigation") {
    std::string s = "a";
    s += std::string {"\xc3\xa9"};
    s += "b";
    CHECK(utf8NextChar(s, 0) == 1);
    CHECK(utf8NextChar(s, 1) == 3);
    CHECK(utf8PrevChar(s, 3) == 1);
    CHECK(utf8Clamp(s, 100) == static_cast<int>(s.size()));
}

TEST_CASE("TextEditUtils: orderedSelection") {
    auto p = orderedSelection(3, 1);
    CHECK(p.first == 1);
    CHECK(p.second == 3);
}

TEST_CASE("TextEditUtils: TextEditSelection ordered and selection state") {
    TextEditSelection sel {.caretByte = 8, .anchorByte = 3};
    CHECK(sel.hasSelection());
    auto const [a, b] = sel.ordered();
    CHECK(a == 3);
    CHECK(b == 8);
}

TEST_CASE("TextEditUtils: shouldCoalesceInsert") {
    std::string const prev = "hello";
    CHECK(shouldCoalesceInsert(prev, 5, "x") == true);
    CHECK(shouldCoalesceInsert(prev, 5, " ") == false);
    CHECK(shouldCoalesceInsert("hello ", 6, "w") == false);
    CHECK(shouldCoalesceInsert(prev, 5, "xy") == false);
}

TEST_CASE("TextEditUtils: lineIndexForByte") {
    std::vector<LineMetrics> lines;
    LineMetrics a {};
    a.byteStart = 0;
    a.byteEnd = 5;
    lines.push_back(a);
    LineMetrics b {};
    b.byteStart = 5;
    b.byteEnd = 10;
    lines.push_back(b);
    CHECK(lineIndexForByte(lines, 0) == 0);
    CHECK(lineIndexForByte(lines, 4) == 0);
    CHECK(lineIndexForByte(lines, 5) == 1);
    CHECK(lineIndexForByte(lines, 9) == 1);
}

TEST_CASE("TextEditUtils: makeTextEditLayoutResult builds line metrics") {
    auto layout = std::make_shared<TextLayout>();
    layout->lines.push_back(TextLayout::LineRange {
        .ctLineIndex = 4,
        .byteStart = 0,
        .byteEnd = 5,
        .lineMinX = 2.f,
        .top = 1.f,
        .bottom = 13.f,
        .baseline = 10.f,
    });

    TextEditLayoutResult const result = makeTextEditLayoutResult(layout, 5, 120.f);
    REQUIRE(result.layout != nullptr);
    REQUIRE(result.lines.size() == 1);
    CHECK(result.textByteCount == 5);
    CHECK(result.contentWidth == doctest::Approx(120.f));
    CHECK(result.lines[0].ctLineIndex == 4);
    CHECK(result.lines[0].byteStart == 0);
    CHECK(result.lines[0].byteEnd == 5);
}

TEST_CASE("TextEditUtils: selectionRects spans wrapped lines") {
    auto layout = std::make_shared<TextLayout>();
    auto storage = std::make_unique<TextLayoutStorage>();
    storage->glyphArena = {1, 2, 3, 4};
    storage->positionArena = {{0.f, 0.f}, {10.f, 0.f}, {0.f, 0.f}, {10.f, 0.f}};

    TextLayout::PlacedRun run0 {};
    run0.run.glyphIds = std::span<std::uint16_t const>(storage->glyphArena.data(), 2);
    run0.run.positions = std::span<Point const>(storage->positionArena.data(), 2);
    run0.run.ascent = 8.f;
    run0.run.descent = 2.f;
    run0.run.width = 20.f;
    run0.origin = {0.f, 8.f};
    run0.utf8Begin = 0;
    run0.utf8End = 2;
    run0.ctLineIndex = 0;

    TextLayout::PlacedRun run1 = run0;
    run1.run.glyphIds = std::span<std::uint16_t const>(storage->glyphArena.data() + 2, 2);
    run1.run.positions = std::span<Point const>(storage->positionArena.data() + 2, 2);
    run1.origin = {0.f, 24.f};
    run1.utf8Begin = 2;
    run1.utf8End = 4;
    run1.ctLineIndex = 1;

    layout->runs = {run0, run1};
    layout->lines = {
        TextLayout::LineRange {.ctLineIndex = 0, .byteStart = 0, .byteEnd = 2, .lineMinX = 0.f, .top = 0.f, .bottom = 10.f, .baseline = 8.f},
        TextLayout::LineRange {.ctLineIndex = 1, .byteStart = 2, .byteEnd = 4, .lineMinX = 0.f, .top = 16.f, .bottom = 26.f, .baseline = 24.f},
    };
    layout->ownedStorage = std::move(storage);

    TextEditLayoutResult const result = makeTextEditLayoutResult(layout, 4, 100.f);
    std::vector<Rect> const rects =
        selectionRects(result, TextEditSelection {.caretByte = 4, .anchorByte = 1}, 5.f, 7.f, 3.f);

    REQUIRE(rects.size() == 2);
    CHECK(rects[0].x == doctest::Approx(15.f));
    CHECK(rects[0].y == doctest::Approx(7.f));
    CHECK(rects[0].width == doctest::Approx(10.f));
    CHECK(rects[0].height == doctest::Approx(13.f));
    CHECK(rects[1].x == doctest::Approx(5.f));
    CHECK(rects[1].y == doctest::Approx(23.f));
    CHECK(rects[1].width == doctest::Approx(20.f));
    CHECK(rects[1].height == doctest::Approx(13.f));
}
