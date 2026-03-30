#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Detail/AnimatedRebuild.hpp>

namespace flux::detail {

void scheduleReactiveRebuildAfterAnimatedChange() {
  if (Application::hasInstance()) {
    Application::instance().markReactiveDirty();
  }
}

} // namespace flux::detail
