#pragma once

#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <memory>

namespace flux {

struct Rectangle;
struct Text;
struct InternalTextLayoutLeaf;
struct PathShape;
struct VStack;
struct HStack;
struct ZStack;
struct Grid;
struct OffsetView;
struct ScrollView;
struct ScaleAroundCenter;
struct Spacer;
struct PopoverCalloutShape;
namespace views {
struct Image;
} // namespace views

namespace detail {

class ComponentBuildContext;

struct ComponentBuildResult {
  std::unique_ptr<scenegraph::SceneNode> node{};
  Size geometrySize{};
  bool hasGeometrySize = false;
};

ComponentBuildResult buildMeasuredFallback(ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);

ComponentBuildResult buildMeasuredComponent(Rectangle const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(Text const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(InternalTextLayoutLeaf const& component,
                                           ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(views::Image const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(PathShape const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(VStack const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(HStack const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(ZStack const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(Grid const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(OffsetView const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(ScrollView const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(ScaleAroundCenter const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(Spacer const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);
ComponentBuildResult buildMeasuredComponent(PopoverCalloutShape const& component, ComponentBuildContext& ctx,
                                           std::unique_ptr<scenegraph::SceneNode> existing);

template<typename C>
ComponentBuildResult buildMeasuredComponent(C const&, ComponentBuildContext& ctx,
                                            std::unique_ptr<scenegraph::SceneNode> existing) {
  return buildMeasuredFallback(ctx, std::move(existing));
}

} // namespace detail

} // namespace flux
