#include <Flux/Reactive/Observer.hpp>

namespace flux {

bool ObserverHandle::isValid() const {
  return id != 0;
}

} // namespace flux
