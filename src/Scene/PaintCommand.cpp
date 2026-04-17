#include <Flux/Scene/PaintCommand.hpp>

#include <Flux/Graphics/Image.hpp>
#include <Flux/Scene/Renderer.hpp>

namespace flux {

void replayPaintCommand(PaintCommand const& cmd, Renderer& renderer) {
  std::visit(
      [&](auto const& op) {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, DrawRectPaintCommand>) {
          renderer.drawRect(op.rect, op.cornerRadius, op.fill, op.stroke, op.shadow);
        } else if constexpr (std::is_same_v<T, DrawTextPaintCommand>) {
          if (op.layout) {
            renderer.drawTextLayout(*op.layout, op.origin);
          }
        } else if constexpr (std::is_same_v<T, DrawImagePaintCommand>) {
          if (op.image) {
            renderer.drawImage(*op.image, op.bounds, op.fillMode, op.cornerRadius, op.opacity);
          }
        } else if constexpr (std::is_same_v<T, DrawPathPaintCommand>) {
          renderer.drawPath(op.path, op.fill, op.stroke, op.shadow);
        } else if constexpr (std::is_same_v<T, DrawLinePaintCommand>) {
          renderer.drawLine(op.from, op.to, op.stroke);
        }
      },
      cmd);
}

} // namespace flux
