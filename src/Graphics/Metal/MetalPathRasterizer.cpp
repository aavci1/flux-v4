#include "Graphics/Metal/MetalPathRasterizer.hpp"
#include "Debug/PerfCounters.hpp"

#include <algorithm>
#include <vector>

namespace flux {

namespace {

template <typename Vec>
void recordCapacityIncrease(std::size_t previousCapacity, Vec const& vec) {
  if (!debug::perf::enabled() || vec.capacity() <= previousCapacity) {
    return;
  }
  using Value = typename Vec::value_type;
  debug::perf::recordRecorderCapacityGrowth(
      static_cast<std::uint64_t>((vec.capacity() - previousCapacity) * sizeof(Value)));
}

} // namespace

void metalPathRasterizeToMesh(Path const& path, FillStyle const& fs, StrokeStyle const& ss, Mat3 const& transform,
                              float dpiScaleX, float dpiScaleY, float opacity, float viewportW, float viewportH,
                              std::vector<PathVertex>& pathVerts, std::vector<MetalPathOp>& pathOps,
                              std::vector<MetalOpRef>& opOrder, BlendMode blendMode) {
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
    std::size_t const previousCapacity = pathVerts.capacity();
    pathVerts.insert(pathVerts.end(), t.vertices.begin(), t.vertices.end());
    recordCapacityIncrease(previousCapacity, pathVerts);
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
          appendVerts(PathFlattener::tessellateStroke(sp, sw, sc, viewportW, viewportH, ss.join, ss.cap));
        }
      }
    }
  }

  const size_t pathEnd = pathVerts.size();
  if (pathEnd > pathBegin) {
    MetalPathOp pop{};
    pop.pathStart = static_cast<std::uint32_t>(pathBegin);
    pop.pathCount = static_cast<std::uint32_t>(pathEnd - pathBegin);
    pop.blendMode = blendMode;
    std::size_t const orderCapacity = opOrder.capacity();
    std::size_t const pathOpCapacity = pathOps.capacity();
    opOrder.push_back(MetalOpRef{
        .kind = MetalOpRef::Path,
        .index = static_cast<std::uint32_t>(pathOps.size()),
    });
    pathOps.push_back(pop);
    recordCapacityIncrease(orderCapacity, opOrder);
    recordCapacityIncrease(pathOpCapacity, pathOps);
  }
}

} // namespace flux
