#include <doctest/doctest.h>

#include "EnvironmentKeyTestSupport.hpp"

#include <Flux/Reactive/Effect.hpp>
#include <Flux/UI/Detail/EnvironmentSlot.hpp>
#include <Flux/UI/EnvironmentBinding.hpp>
#include <Flux/UI/EnvironmentKeys.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace flux {

FLUX_DEFINE_ENVIRONMENT_KEY(FirstEnvironmentTestKey, int, 10);
FLUX_DEFINE_ENVIRONMENT_KEY(SecondEnvironmentTestKey, int, 20);
FLUX_DEFINE_ENVIRONMENT_KEY(StringEnvironmentTestKey, std::string, std::string{"fallback"});

template<std::size_t>
struct ManyEnvironmentSlotTag {};

} // namespace flux

namespace flux::tests {

struct DestructionCounter {
  int* destroyed = nullptr;

  bool operator==(DestructionCounter const& other) const {
    return destroyed == other.destroyed;
  }

  ~DestructionCounter() {
    if (destroyed) {
      ++*destroyed;
    }
  }
};

} // namespace flux::tests

namespace {

template<std::size_t... I>
std::array<std::uint16_t, sizeof...(I)> allocateManyEnvironmentSlots(std::index_sequence<I...>) {
  return {flux::detail::allocateEnvironmentSlot(typeid(flux::ManyEnvironmentSlotTag<I>))...};
}

} // namespace

TEST_CASE("environment keys allocate distinct stable slots") {
  std::uint16_t const first = flux::EnvironmentKey<flux::FirstEnvironmentTestKey>::slot().index();
  std::uint16_t const second = flux::EnvironmentKey<flux::SecondEnvironmentTestKey>::slot().index();
  std::uint16_t const shared = flux::EnvironmentKey<flux::SharedEnvironmentTestKey>::slot().index();

  CHECK(first != second);
  CHECK(shared == flux::tests::sharedEnvironmentTestKeyIndexFromOtherTranslationUnit());
}

TEST_CASE("environment slot registry reuses existing assignments") {
  struct LocalSlotTag {};

  std::uint16_t const first = flux::detail::allocateEnvironmentSlot(typeid(LocalSlotTag));
  std::uint16_t const second = flux::detail::allocateEnvironmentSlot(typeid(LocalSlotTag));
  CHECK(first == second);
}

TEST_CASE("environment slot registry assigns many distinct indices") {
  auto const indices = allocateManyEnvironmentSlots(std::make_index_sequence<100>{});
  for (std::size_t i = 0; i < indices.size(); ++i) {
    for (std::size_t j = i + 1; j < indices.size(); ++j) {
      CHECK(indices[i] != indices[j]);
    }
  }
}

TEST_CASE("environment entries store values and reject mismatched types") {
  flux::detail::EnvironmentEntry entry;
  entry.setValue<int>(42);

  REQUIRE(entry.kind() == flux::detail::EnvironmentEntryKind::Value);
  REQUIRE(entry.asValue<int>() != nullptr);
  CHECK(*entry.asValue<int>() == 42);
  CHECK(entry.asValue<float>() == nullptr);
  CHECK(entry.asSignal<int>() == nullptr);
}

TEST_CASE("environment entries store signal handles by identity") {
  flux::detail::EnvironmentEntry lhs;
  flux::detail::EnvironmentEntry rhs;
  flux::detail::EnvironmentEntry different;
  flux::Reactive::Signal<int> signal{3};

  lhs.setSignal<int>(signal);
  rhs.setSignal<int>(signal);
  different.setSignal<int>(flux::Reactive::Signal<int>{3});

  REQUIRE(lhs.asSignal<int>() != nullptr);
  CHECK(lhs.asSignal<int>()->peek() == 3);
  CHECK(lhs.equals(rhs));
  CHECK_FALSE(lhs.equals(different));
}

TEST_CASE("environment entries copy, move, and destroy stored values") {
  int destroyed = 0;
  {
    flux::detail::EnvironmentEntry entry;
    entry.setValue(flux::tests::DestructionCounter{&destroyed});
    flux::detail::EnvironmentEntry copy = entry;
    CHECK(copy.asValue<flux::tests::DestructionCounter>() != nullptr);

    flux::detail::EnvironmentEntry moved = std::move(copy);
    CHECK(moved.asValue<flux::tests::DestructionCounter>() != nullptr);
    CHECK(copy.kind() == flux::detail::EnvironmentEntryKind::None);
  }
  CHECK(destroyed >= 3);
}

TEST_CASE("environment entry move is noexcept") {
  static_assert(std::is_nothrow_move_constructible_v<flux::detail::EnvironmentEntry>);
  static_assert(std::is_nothrow_move_assignable_v<flux::detail::EnvironmentEntry>);
}

TEST_CASE("environment binding resolves defaults, values, and signals") {
  using namespace flux::tests;

  flux::EnvironmentBinding binding;
  CHECK(binding.value<flux::FirstEnvironmentTestKey>() == 10);

  auto darkBinding = binding.withValue<flux::ThemeKey>(flux::Theme::dark());
  CHECK(darkBinding.value<flux::ThemeKey>() == flux::Theme::dark());
  CHECK(binding.value<flux::ThemeKey>() == flux::Theme::light());

  flux::Reactive::Signal<flux::Theme> theme{flux::Theme::light()};
  auto signalBinding = binding.withSignal<flux::ThemeKey>(theme);
  auto signal = signalBinding.signal<flux::ThemeKey>();
  REQUIRE(signal.has_value());
  CHECK(signal->peek() == flux::Theme::light());
  CHECK(signalBinding.value<flux::ThemeKey>() == flux::Theme::light());

  theme = flux::Theme::dark();
  CHECK(signalBinding.value<flux::ThemeKey>() == flux::Theme::dark());
}

TEST_CASE("environment binding reuses entries when rebinding matching values") {
  flux::EnvironmentBinding original =
      flux::EnvironmentBinding{}.withValue<flux::ThemeKey>(flux::Theme::light());

  flux::EnvironmentBinding rebound =
      original.withValue<flux::ThemeKey>(flux::Theme::light());

  CHECK(rebound.internalEntriesPointer() == original.internalEntriesPointer());
}

TEST_CASE("environment binding reuses entries when rebinding matching signals") {
  flux::Reactive::Signal<flux::Theme> themeSignal{flux::Theme::light()};
  flux::EnvironmentBinding original =
      flux::EnvironmentBinding{}.withSignal<flux::ThemeKey>(themeSignal);

  flux::EnvironmentBinding rebound =
      original.withSignal<flux::ThemeKey>(themeSignal);

  CHECK(rebound.internalEntriesPointer() == original.internalEntriesPointer());
}

TEST_CASE("signal-backed environment binding participates in reactive tracking") {
  flux::Reactive::Signal<flux::Theme> theme{flux::Theme::light()};
  flux::EnvironmentBinding binding =
      flux::EnvironmentBinding{}.withSignal<flux::ThemeKey>(theme);

  int runs = 0;
  flux::Color observed{};
  flux::Reactive::Effect effect{[&] {
    ++runs;
    observed = binding.value<flux::ThemeKey>().labelColor;
  }};

  CHECK(runs == 1);
  CHECK(observed == flux::Theme::light().labelColor);

  theme = flux::Theme::dark();

  CHECK(runs == 2);
  CHECK(observed == flux::Theme::dark().labelColor);
}
