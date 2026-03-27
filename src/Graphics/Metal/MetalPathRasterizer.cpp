#include "Graphics/Metal/MetalPathRasterizer.hpp"

#include <algorithm>
#include <vector>

namespace flux {

void metalPathRasterizeToMesh(Path const& path, FillStyle const& fs, StrokeStyle const& ss, Mat3 const& transform,
                              float dpiScaleX, float dpiScaleY, float opacity, float viewportW, float viewportH,
                              std::vector<PathVertex>& pathVerts, std::vector<MetalDrawOp>& ops) {
  if (path.isEmpty() || viewportW < 1.f || viewportH < 1.f) {
    return;
  }

  const float s = std::min(dpiScaleX, dpiScaleY);
  const size_t pathBegin = pathVerts.size();

  auto subpaths = PathFlattener::flattenSubpaths(path);
  for (auto& sp : subpaths) {
    for (auto& p : sp) {
      Point q = transform.apply(p);
      p = {q.x * dpiScaleX, q.y * dpiScaleY};
    }
  }

  auto appendVerts = [&pathVerts](TessellatedPath&& t) {
    if (t.vertices.empty()) {
      return;
    }
    pathVerts.insert(pathVerts.end(), t.vertices.begin(), t.vertices.end());
  };

  if (!fs.isNone()) {
    Color fc{};
    if (fs.solidColor(&fc)) {
      fc.a *= opacity;
      if (subpaths.size() > 1) {
        std::vector<std::vector<Point>> nonempty;
        nonempty.reserve(subpaths.size());
        for (const auto& s : subpaths) {
          if (s.size() >= 3) {
            nonempty.push_back(s);
          }
        }
        if (!nonempty.empty()) {
          appendVerts(PathFlattener::tessellateFillContours(nonempty, fc, viewportW, viewportH,
                                                            PathFlattener::tessWindingFromFillRule(fs.fillRule)));
        }
      } else {
        for (auto& s : subpaths) {
          if (s.size() >= 3) {
            appendVerts(PathFlattener::tessellateFill(s, fc, viewportW, viewportH));
          }
        }
      }
    }
  }

  if (!ss.isNone()) {
    Color sc{};
    if (ss.solidColor(&sc)) {
      sc.a *= opacity;
      const float sw = ss.width * s;
      for (const auto& sp : subpaths) {
        if (sp.size() >= 2) {
          appendVerts(PathFlattener::tessellateStroke(sp, sw, sc, viewportW, viewportH));
        }
      }
    }
  }

  const size_t pathEnd = pathVerts.size();
  if (pathEnd > pathBegin) {
    MetalDrawOp pop{};
    pop.kind = MetalDrawOp::PathMesh;
    pop.pathStart = static_cast<std::uint32_t>(pathBegin);
    pop.pathCount = static_cast<std::uint32_t>(pathEnd - pathBegin);
    ops.push_back(pop);
  }
}

} // namespace flux
