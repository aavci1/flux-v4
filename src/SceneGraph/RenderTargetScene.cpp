#include <Flux/Graphics/RenderTarget.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>

namespace flux {

void RenderTarget::renderScene(scenegraph::SceneGraph const& scene) {
  scenegraph::SceneRenderer renderer(canvas());
  renderer.render(scene);
}

} // namespace flux
