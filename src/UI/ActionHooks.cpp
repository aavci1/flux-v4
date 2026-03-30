#include <Flux/Detail/Runtime.hpp>
#include <Flux/UI/Hooks.hpp>
#include <Flux/UI/StateStore.hpp>

#include <cassert>
#include <functional>
#include <string>

namespace flux {

void useViewAction(std::string const& name, std::function<void()> handler, std::function<bool()> isEnabled) {
  Runtime* rt = Runtime::current();
  StateStore* store = StateStore::current();
  assert(rt && "useViewAction called outside of a build pass");
  assert(store && "useViewAction called outside of a build pass");
  if (store->overlayScope().has_value()) {
    assert(false && "useViewAction is not supported in overlay subtrees");
    return;
  }
  rt->actionRegistryForBuild().registerViewClaim(store->currentComponentKey(), name, std::move(handler),
                                                 std::move(isEnabled));
}

void useWindowAction(std::string const& name, std::function<void()> handler, std::function<bool()> isEnabled) {
  Runtime* rt = Runtime::current();
  assert(rt && "useWindowAction called outside of a build pass");
  rt->actionRegistryForBuild().registerWindowAction(name, std::move(handler), std::move(isEnabled));
}

} // namespace flux
