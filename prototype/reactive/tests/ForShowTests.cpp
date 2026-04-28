#include "For.hpp"
#include "Show.hpp"
#include "Signal.hpp"
#include "Test.hpp"

#include <memory>
#include <string>
#include <vector>

using namespace fluxv5;

namespace {

struct Item {
  int id = 0;
  int value = 0;

  bool operator==(Item const&) const = default;
};

struct RowOutput {
  int id = 0;
  Signal<std::size_t> index;
  Signal<int> localState;
};

} // namespace

static void runForShowTests() {
  Signal<std::vector<Item>> items(std::vector<Item>{
    {.id = 1, .value = 10},
    {.id = 2, .value = 20},
    {.id = 3, .value = 30},
  });

  int factoryRuns = 0;
  int disposedRows = 0;
  auto rows = For<Item>(
    items,
    [](Item const& item) {
      return item.id;
    },
    [&](Item const& item, Signal<std::size_t> index) {
      ++factoryRuns;
      onCleanup([&] {
        ++disposedRows;
      });
      return std::make_shared<RowOutput>(
        RowOutput{.id = item.id, .index = index, .localState = Signal<int>(item.value)});
    });

  V5_CHECK(rows.size() == 3);
  V5_CHECK(factoryRuns == 3);
  rows.outputs()[1]->localState.set(200);

  items.set(std::vector<Item>{
    {.id = 3, .value = 300},
    {.id = 2, .value = 250},
    {.id = 1, .value = 100},
  });

  V5_CHECK(rows.size() == 3);
  V5_CHECK(factoryRuns == 3);
  V5_CHECK(rows.outputs()[0]->id == 3);
  V5_CHECK(rows.outputs()[0]->index.get() == 0);
  V5_CHECK(rows.outputs()[1]->id == 2);
  V5_CHECK(rows.outputs()[1]->index.get() == 1);
  V5_CHECK(rows.outputs()[1]->localState.get() == 200);
  V5_CHECK(disposedRows == 0);

  items.set(std::vector<Item>{
    {.id = 4, .value = 40},
  });
  V5_CHECK(rows.size() == 1);
  V5_CHECK(factoryRuns == 4);
  V5_CHECK(disposedRows == 3);

  Signal<bool> showThen(true);
  int thenRuns = 0;
  int elseRuns = 0;
  int branchCleanups = 0;
  auto show = Show(
    [&] {
      return showThen.get();
    },
    [&] {
      ++thenRuns;
      onCleanup([&] {
        ++branchCleanups;
      });
      return std::string("then");
    },
    [&] {
      ++elseRuns;
      onCleanup([&] {
        ++branchCleanups;
      });
      return std::string("else");
    });

  V5_CHECK(show.output() == "then");
  V5_CHECK(thenRuns == 1);
  V5_CHECK(elseRuns == 0);

  showThen.set(false);
  V5_CHECK(show.output() == "else");
  V5_CHECK(thenRuns == 1);
  V5_CHECK(elseRuns == 1);
  V5_CHECK(branchCleanups == 1);

  showThen.set(false);
  V5_CHECK(elseRuns == 1);
}

V5_TEST_MAIN(runForShowTests)
