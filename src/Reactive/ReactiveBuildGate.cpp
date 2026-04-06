#include <Flux/Reactive/Detail/ReactiveBuildGate.hpp>

#include <Flux/UI/StateStore.hpp>

namespace flux::detail {

namespace {

std::uint64_t gContentEpoch = 0;
thread_local bool gStateMutationDuringBuild = false;

} // namespace

void bumpReactiveContentEpoch() {
  ++gContentEpoch;
}

std::uint64_t reactiveContentEpoch() noexcept {
  return gContentEpoch;
}

void markStateMutationDuringBuildIfActive() {
  if (StateStore::current()) {
    gStateMutationDuringBuild = true;
  }
}

bool peekStateMutationDuringBuild() noexcept {
  return gStateMutationDuringBuild;
}

void clearStateMutationDuringBuild() noexcept {
  gStateMutationDuringBuild = false;
}

} // namespace flux::detail
