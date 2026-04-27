#include <doctest/doctest.h>

#include <Flux/Core/KeyCodes.hpp>
#include <Flux/Core/Shortcut.hpp>
#include <Flux/UI/ActionRegistry.hpp>

#include <unordered_map>

TEST_CASE("runtime tests are parked for the v5 mount runtime rewrite") {
  CHECK(true);
}

TEST_CASE("action registry unregisters window actions by id") {
  flux::ActionRegistry registry;
  int fired = 0;
  flux::ActionId const id = registry.registerWindowAction("demo.save", [&] {
    ++fired;
  });

  std::unordered_map<std::string, flux::ActionDescriptor> descriptors;
  descriptors.emplace("demo.save", flux::ActionDescriptor{
      .label = "Save",
      .shortcut = flux::shortcuts::Save,
  });

  CHECK(registry.dispatchShortcut({}, flux::keys::S, flux::Modifiers::Meta, descriptors));
  CHECK(fired == 1);

  registry.unregister(id);

  CHECK_FALSE(registry.dispatchShortcut({}, flux::keys::S, flux::Modifiers::Meta, descriptors));
  CHECK(fired == 1);
}

TEST_CASE("action registry unregisters view claims by id") {
  flux::ActionRegistry registry;
  int fired = 0;
  flux::ActionId const id = registry.registerViewClaim({}, "demo.save", [&] {
    ++fired;
  });

  std::unordered_map<std::string, flux::ActionDescriptor> descriptors;
  descriptors.emplace("demo.save", flux::ActionDescriptor{
      .label = "Save",
      .shortcut = flux::shortcuts::Save,
  });

  CHECK(registry.dispatchShortcut({}, flux::keys::S, flux::Modifiers::Meta, descriptors));
  CHECK(fired == 1);

  registry.unregister(id);

  CHECK_FALSE(registry.dispatchShortcut({}, flux::keys::S, flux::Modifiers::Meta, descriptors));
  CHECK(fired == 1);
}
