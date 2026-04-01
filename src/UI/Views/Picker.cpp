#include <Flux/Core/Types.hpp>

namespace flux {

CornerRadius pickerMenuRowCorners(std::size_t rowIndex, std::size_t rowCount, float r) {
  if (rowCount == 0 || r <= 0.f) {
    return CornerRadius{};
  }
  if (rowCount == 1) {
    return CornerRadius{r};
  }
  if (rowIndex == 0) {
    return CornerRadius{r, r, 0.f, 0.f};
  }
  if (rowIndex == rowCount - 1) {
    return CornerRadius{0.f, 0.f, r, r};
  }
  return CornerRadius{};
}

} // namespace flux
