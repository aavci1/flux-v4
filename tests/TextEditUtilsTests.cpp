#include <doctest/doctest.h>

#include <Flux/UI/Views/TextEditUtils.hpp>

#include <string>

using namespace flux;
using namespace flux::detail;

TEST_CASE("TextEditUtils: utf8 navigation") {
  std::string s = "a";
  s += std::string{"\xc3\xa9"};
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

TEST_CASE("TextEditUtils: shouldCoalesceInsert") {
  std::string const prev = "hello";
  CHECK(shouldCoalesceInsert(prev, 5, "x") == true);
  CHECK(shouldCoalesceInsert(prev, 5, " ") == false);
  CHECK(shouldCoalesceInsert("hello ", 6, "w") == false);
  CHECK(shouldCoalesceInsert(prev, 5, "xy") == false);
}

TEST_CASE("TextEditUtils: lineIndexForByte") {
  std::vector<LineMetrics> lines;
  LineMetrics a{};
  a.byteStart = 0;
  a.byteEnd = 5;
  lines.push_back(a);
  LineMetrics b{};
  b.byteStart = 5;
  b.byteEnd = 10;
  lines.push_back(b);
  CHECK(lineIndexForByte(lines, 0) == 0);
  CHECK(lineIndexForByte(lines, 4) == 0);
  CHECK(lineIndexForByte(lines, 5) == 1);
  CHECK(lineIndexForByte(lines, 9) == 1);
}
