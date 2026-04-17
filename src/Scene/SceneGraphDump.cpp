#include <Flux/Scene/SceneGraphDump.hpp>

#include <Flux/Core/Types.hpp>

#include <ostream>
#include <string>
#include <variant>

namespace flux {
namespace {

std::string pad(int depth) { return std::string(static_cast<std::size_t>(depth) * 2, ' '); }

void fmtRect(std::ostream& os, Rect const& r) {
  os << '(' << r.x << ", " << r.y << ", " << r.width << " x " << r.height << ')';
}

void fmtPoint(std::ostream& os, Point const& p) { os << '(' << p.x << ", " << p.y << ')'; }

void fmtNodeId(std::ostream& os, NodeId id) {
  if (!id.isValid()) {
    os << "invalid";
    return;
  }
  os << id.index << '/' << id.generation;
}

void fmtMat3(std::ostream& os, Mat3 const& m) {
  os << '[';
  for (int i = 0; i < 9; ++i) {
    if (i > 0) {
      os << (i % 3 == 0 ? "; " : ", ");
    }
    os << m.m[i];
  }
  os << ']';
}

void fmtColor(std::ostream& os, Color const& c) {
  os << "rgba(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ')';
}

void dumpNode(SceneGraph const& graph, NodeId id, int depth, std::ostream& os) {
  std::string const p = pad(depth);
  if (!id.isValid()) {
    os << p << "(invalid NodeId)\n";
    return;
  }
  LegacySceneNode const* sn = graph.get(id);
  if (!sn) {
    os << p << "NodeId ";
    fmtNodeId(os, id);
    os << " (missing from store)\n";
    return;
  }

  std::visit(
      [&](auto const& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, LayerNode>) {
          os << p << "Layer ";
          fmtNodeId(os, id);
          os << "  opacity=" << node.opacity << "  blend=" << static_cast<int>(node.blendMode);
          os << "  transform=";
          fmtMat3(os, node.transform);
          if (node.clip.has_value()) {
            os << "  clip=";
            fmtRect(os, *node.clip);
          } else {
            os << "  clip=(none)";
          }
          os << "  children=" << node.children.size() << '\n';
          for (NodeId child : node.children) {
            dumpNode(graph, child, depth + 1, os);
          }
        } else if constexpr (std::is_same_v<T, RectNode>) {
          os << p << "Rect ";
          fmtNodeId(os, id);
          os << "  bounds=";
          fmtRect(os, node.bounds);
          os << "  cornerRadius tl=" << node.cornerRadius.topLeft << " tr=" << node.cornerRadius.topRight
             << " br=" << node.cornerRadius.bottomRight << " bl=" << node.cornerRadius.bottomLeft;
          Color fc{};
          if (node.fill.solidColor(&fc)) {
            os << "  fill=";
            fmtColor(os, fc);
          } else {
            os << "  fill=(none)";
          }
          Color sc{};
          if (node.stroke.solidColor(&sc)) {
            os << "  stroke=";
            fmtColor(os, sc);
            os << " w=" << node.stroke.width;
          } else {
            os << "  stroke=(none)";
          }
          os << '\n';
        } else if constexpr (std::is_same_v<T, TextNode>) {
          os << p << "Text ";
          fmtNodeId(os, id);
          os << "  origin=";
          fmtPoint(os, node.origin);
          if (node.layout) {
            os << "  measured=" << node.layout->measuredSize.width << " x " << node.layout->measuredSize.height
               << "  runs=" << node.layout->runs.size();
          } else {
            os << "  layout=(null)";
          }
          os << '\n';
        } else if constexpr (std::is_same_v<T, ImageNode>) {
          os << p << "Image ";
          fmtNodeId(os, id);
          os << "  bounds=";
          fmtRect(os, node.bounds);
          os << "  opacity=" << node.opacity << "  image=" << (node.image ? "set" : "null") << '\n';
        } else if constexpr (std::is_same_v<T, PathNode>) {
          os << p << "Path ";
          fmtNodeId(os, id);
          os << "  commands=" << node.path.commandCount() << '\n';
        } else if constexpr (std::is_same_v<T, LineNode>) {
          os << p << "Line ";
          fmtNodeId(os, id);
          os << "  from=";
          fmtPoint(os, node.from);
          os << "  to=";
          fmtPoint(os, node.to);
          Color sc{};
          if (node.stroke.solidColor(&sc)) {
            os << "  stroke=";
            fmtColor(os, sc);
            os << " w=" << node.stroke.width;
          }
          os << '\n';
        } else if constexpr (std::is_same_v<T, CustomRenderNode>) {
          os << p << "CustomRender ";
          fmtNodeId(os, id);
          os << "  frame=";
          fmtRect(os, node.frame);
          os << '\n';
        }
      },
      *sn);
}

} // namespace

void dumpSceneGraph(SceneGraph const& graph, std::ostream& os) {
  os << "---- SceneGraph dump ----\n";
  os << "root: ";
  fmtNodeId(os, graph.root());
  os << '\n';
  dumpNode(graph, graph.root(), 0, os);
  os << "---- end SceneGraph dump ----\n";
  os << std::flush;
}

} // namespace flux
