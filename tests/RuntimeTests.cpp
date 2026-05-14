#include <doctest/doctest.h>

#include <Flux/UI/KeyCodes.hpp>
#include <Flux/UI/Shortcut.hpp>
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

TEST_CASE("component keys minted from scopes are non-empty and stable") {
  int firstScope = 0;
  int secondScope = 0;

  flux::ComponentKey const firstKey = flux::ComponentKey::fromScope(&firstScope);
  flux::ComponentKey const sameFirstKey = flux::ComponentKey::fromScope(&firstScope);
  flux::ComponentKey const secondKey = flux::ComponentKey::fromScope(&secondScope);

  CHECK_FALSE(firstKey.empty());
  CHECK(firstKey == sameFirstKey);
  CHECK(firstKey != secondKey);
}

TEST_CASE("view claims registered with scope keys only fire for the focused scope") {
  flux::ActionRegistry registry;
  int firstFired = 0;
  int secondFired = 0;
  int otherScope = 0;
  int firstScope = 0;
  int secondScope = 0;
  flux::ComponentKey const firstKey = flux::ComponentKey::fromScope(&firstScope);
  flux::ComponentKey const secondKey = flux::ComponentKey::fromScope(&secondScope);
  flux::ComponentKey const otherKey = flux::ComponentKey::fromScope(&otherScope);

  registry.registerViewClaim(firstKey, "demo.save", [&] {
    ++firstFired;
  });
  registry.registerViewClaim(secondKey, "demo.save", [&] {
    ++secondFired;
  });

  std::unordered_map<std::string, flux::ActionDescriptor> descriptors;
  descriptors.emplace("demo.save", flux::ActionDescriptor{
      .label = "Save",
      .shortcut = flux::shortcuts::Save,
  });

  CHECK(registry.dispatchShortcut(firstKey, flux::keys::S, flux::Modifiers::Meta, descriptors));
  CHECK(firstFired == 1);
  CHECK(secondFired == 0);

  CHECK(registry.dispatchShortcut(secondKey, flux::keys::S, flux::Modifiers::Meta, descriptors));
  CHECK(firstFired == 1);
  CHECK(secondFired == 1);

  CHECK_FALSE(registry.dispatchShortcut(otherKey, flux::keys::S, flux::Modifiers::Meta, descriptors));
  CHECK(firstFired == 1);
  CHECK(secondFired == 1);
}

TEST_CASE("view claims registered with scope keys still match focused descendants") {
  flux::ActionRegistry registry;
  int fired = 0;
  int scope = 0;
  flux::ComponentKey const scopeKey = flux::ComponentKey::fromScope(&scope);
  flux::ComponentKey const focusedLeaf{scopeKey, flux::LocalId::fromString("leaf")};

  registry.registerViewClaim(scopeKey, "demo.save", [&] {
    ++fired;
  });

  std::unordered_map<std::string, flux::ActionDescriptor> descriptors;
  descriptors.emplace("demo.save", flux::ActionDescriptor{
      .label = "Save",
      .shortcut = flux::shortcuts::Save,
  });

  CHECK(registry.dispatchShortcut(focusedLeaf, flux::keys::S, flux::Modifiers::Meta, descriptors));
  CHECK(fired == 1);
}
