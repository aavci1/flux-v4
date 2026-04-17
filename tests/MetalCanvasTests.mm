#include <doctest/doctest.h>

#include "Graphics/CoreTextSystem.hpp"
#include "Graphics/Metal/MetalCanvas.hpp"

#include <Flux/Graphics/Image.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/SceneRenderer.hpp>

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/QuartzCore.h>

#include <filesystem>
#include <memory>
#include <string>

namespace {

using namespace flux;

struct TestSurface {
  NSWindow* window = nil;
  NSView* view = nil;
  CAMetalLayer* layer = nil;
};

static TestSurface makeSurface() {
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];

  NSRect const frame = NSMakeRect(-10000.f, -10000.f, 640.f, 480.f);
  TestSurface surface;
  surface.window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:NSWindowStyleMaskBorderless
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
  surface.view = [[NSView alloc] initWithFrame:NSMakeRect(0.f, 0.f, 640.f, 480.f)];
  surface.view.wantsLayer = YES;
  surface.layer = [CAMetalLayer layer];
  surface.layer.device = MTLCreateSystemDefaultDevice();
  surface.layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  surface.layer.contentsScale = 2.f;
  surface.layer.maximumDrawableCount = 3;
  surface.layer.allowsNextDrawableTimeout = YES;
  surface.layer.frame = surface.view.bounds;
  surface.layer.drawableSize = CGSizeMake(1280.f, 960.f);
  surface.view.layer = surface.layer;
  [surface.window setContentView:surface.view];
  [surface.window orderFrontRegardless];
  [CATransaction flush];
  return surface;
}

static std::filesystem::path imageFixturePath() {
  std::filesystem::path path = std::filesystem::path(__FILE__).parent_path();
  path /= "../examples/image-demo/test.png";
  return std::filesystem::weakly_canonical(path);
}

static std::shared_ptr<TextLayout const> makeLabel(CoreTextSystem& textSystem, std::string const& text) {
  Font font{};
  font.family = ".AppleSystemUIFont";
  font.size = 13.f;
  font.weight = 400.f;
  return textSystem.layout(text, font, Colors::white, 120.f, {});
}

static SceneGraph makeStressScene(CoreTextSystem& textSystem, std::shared_ptr<Image> const& image) {
  SceneGraph graph;

  for (int i = 0; i < 256; ++i) {
    NodeId const layerId = graph.addLayer(graph.root(), LayerNode{});
    float const x = static_cast<float>((i % 32) * 18);
    float const y = static_cast<float>((i / 32) * 18);
    graph.addRect(layerId, RectNode{
                               .bounds = flux::Rect::sharp(x, y, 14.f, 14.f),
                               .cornerRadius = CornerRadius{3.f, 3.f, 3.f, 3.f},
                               .fill = FillStyle::solid(Color{
                                   static_cast<float>((17 * i) % 255) / 255.f,
                                   static_cast<float>((37 * i) % 255) / 255.f,
                                   static_cast<float>((53 * i) % 255) / 255.f,
                                   1.f,
                               }),
                               .stroke = StrokeStyle::none(),
                               .shadow = ShadowStyle::none(),
                           });
  }

  for (int i = 0; i < 64; ++i) {
    NodeId const layerId = graph.addLayer(graph.root(), LayerNode{});
    float const x = static_cast<float>((i % 8) * 72);
    float const y = 170.f + static_cast<float>(i / 8) * 16.f;
    graph.addText(layerId, TextNode{
                               .layout = makeLabel(textSystem, "Row " + std::to_string(i)),
                               .origin = flux::Point{x, y},
                               .allocation = flux::Rect::sharp(x, y - 12.f, 64.f, 14.f),
                           });
  }

  Path triangle;
  triangle.moveTo({320.f, 40.f});
  triangle.lineTo({420.f, 180.f});
  triangle.lineTo({240.f, 180.f});
  triangle.close();
  graph.addPath(graph.root(), PathNode{
                                 .path = triangle,
                                 .fill = FillStyle::solid(Color{0.2f, 0.6f, 0.9f, 1.f}),
                                 .stroke = StrokeStyle::none(),
                                 .shadow = ShadowStyle::none(),
                             });

  if (image) {
    for (int i = 0; i < 9; ++i) {
      NodeId const layerId = graph.addLayer(graph.root(), LayerNode{});
      float const x = static_cast<float>((i % 3) * 88);
      float const y = 320.f + static_cast<float>(i / 3) * 88.f;
      graph.addImage(layerId, ImageNode{
                                  .image = image,
                                  .bounds = flux::Rect::sharp(x, y, 72.f, 72.f),
                                  .fillMode = ImageFillMode::Cover,
                                  .cornerRadius = CornerRadius{6.f, 6.f, 6.f, 6.f},
                                  .opacity = 1.f,
                              });
    }
  }

  return graph;
}

} // namespace

TEST_CASE("MetalCanvas can render multiple queued frames without arena aliasing regressions") {
#if !defined(__APPLE__)
  SUCCEED();
#else
  @autoreleasepool {
    CoreTextSystem textSystem;
    TestSurface surface = makeSurface();
    auto canvas = createMetalCanvas(nullptr, (__bridge void*)surface.layer, 0, textSystem);
    REQUIRE(canvas);
    canvas->resize(640, 480);
    canvas->updateDpiScale(2.f, 2.f);

    std::shared_ptr<Image> image = loadImageFromFile(imageFixturePath().string(), canvas->gpuDevice());
    SceneGraph graph = makeStressScene(textSystem, image);
    SceneRenderer renderer;

    for (int frame = 0; frame < 18; ++frame) {
      if (auto* root = graph.node<LayerNode>(graph.root())) {
        root->transform = Mat3::translate(0.f, static_cast<float>(frame % 3));
        graph.markPaintDirty(root->id);
      }
      canvas->beginFrame();
      renderer.render(graph, *canvas, Color{0.08f, 0.09f, 0.11f, 1.f});
      canvas->present();
    }

    waitForCanvasLastPresentComplete(canvas.get());
    CHECK(true);
  }
#endif
}
