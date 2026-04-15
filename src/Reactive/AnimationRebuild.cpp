#include <Flux/Core/Application.hpp>
#include <Flux/Reactive/Detail/AnimationRebuild.hpp>

namespace flux::detail {

void scheduleReactiveRebuildAfterAnimationChange() {
  if (Application::hasInstance()) {
    Application::instance().markReactiveDirty();
  }
}

} // namespace flux::detail
