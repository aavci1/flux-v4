#include <doctest/doctest.h>

#include "Graphics/CoreTextSystem.hpp"
#include "Graphics/Metal/MetalCanvas.hpp"

#include <Flux/Graphics/Image.hpp>
#include <Flux/SceneGraph/GroupNode.hpp>
#include <Flux/SceneGraph/ImageNode.hpp>
#include <Flux/SceneGraph/PathNode.hpp>
#include <Flux/SceneGraph/RectNode.hpp>
#include <Flux/SceneGraph/SceneGraph.hpp>
#include <Flux/SceneGraph/SceneRenderer.hpp>
#include <Flux/SceneGraph/TextNode.hpp>

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/QuartzCore.h>

#include <filesystem>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace flux;
using namespace flux::scenegraph;

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
  surface.layer.displaySyncEnabled = NO;
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

struct StressScene {
  std::unique_ptr<SceneGraph> graph;
  SceneNode* animatedGroup = nullptr;
};

static StressScene makeStressScene(CoreTextSystem& textSystem, std::shared_ptr<Image> const& image) {
  auto graph = std::make_unique<SceneGraph>();
  auto root = std::make_unique<GroupNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
  root->appendChild(std::make_unique<RectNode>(
      flux::Rect{0.f, 0.f, 640.f, 480.f},
      FillStyle::solid(Color{0.08f, 0.09f, 0.11f, 1.f})
  ));

  auto animatedGroup = std::make_unique<GroupNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
  SceneNode* animatedGroupPtr = animatedGroup.get();

  for (int i = 0; i < 256; ++i) {
    animatedGroup->appendChild(std::make_unique<RectNode>(
        flux::Rect{
            static_cast<float>((i % 32) * 18),
            static_cast<float>((i / 32) * 18),
            14.f,
            14.f,
        },
        FillStyle::solid(Color{
            static_cast<float>((17 * i) % 255) / 255.f,
            static_cast<float>((37 * i) % 255) / 255.f,
            static_cast<float>((53 * i) % 255) / 255.f,
            1.f,
        }),
        StrokeStyle::none(),
        CornerRadius{3.f, 3.f, 3.f, 3.f}
    ));
  }

  for (int i = 0; i < 64; ++i) {
    animatedGroup->appendChild(std::make_unique<TextNode>(
        flux::Rect{
            static_cast<float>((i % 8) * 72),
            170.f + static_cast<float>(i / 8) * 16.f,
            64.f,
            14.f,
        },
        makeLabel(textSystem, "Row " + std::to_string(i))
    ));
  }

  Path triangle;
  triangle.moveTo({0.f, 0.f});
  triangle.lineTo({100.f, 140.f});
  triangle.lineTo({-80.f, 140.f});
  triangle.close();
  animatedGroup->appendChild(std::make_unique<PathNode>(
      flux::Rect{320.f, 40.f, 180.f, 140.f},
      triangle,
      FillStyle::solid(Color{0.2f, 0.6f, 0.9f, 1.f}),
      StrokeStyle::none(),
      ShadowStyle::none()
  ));

  if (image) {
    std::shared_ptr<Image const> constImage = image;
    for (int i = 0; i < 9; ++i) {
      animatedGroup->appendChild(std::make_unique<ImageNode>(
          flux::Rect{
              static_cast<float>((i % 3) * 88),
              320.f + static_cast<float>(i / 3) * 88.f,
              72.f,
              72.f,
          },
          constImage
      ));
    }
  }

  root->appendChild(std::move(animatedGroup));
  graph->setRoot(std::move(root));
  return StressScene{.graph = std::move(graph), .animatedGroup = animatedGroupPtr};
}

static std::uint8_t capturedChannel(std::vector<std::uint8_t> const& pixels, std::uint32_t width,
                                    std::uint32_t x, std::uint32_t y, int channel) {
  std::size_t const idx =
      (static_cast<std::size_t>(y) * width + x) * 4u + static_cast<std::size_t>(channel);
  return pixels[idx];
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
    StressScene scene = makeStressScene(textSystem, image);
    REQUIRE(scene.animatedGroup != nullptr);

    SceneRenderer renderer{*canvas};
    for (int frame = 0; frame < 18; ++frame) {
      scene.animatedGroup->setPosition(flux::Point{0.f, static_cast<float>(frame % 3)});
      canvas->beginFrame();
      canvas->clear(Color{0.08f, 0.09f, 0.11f, 1.f});
      renderer.render(*scene.graph);
      canvas->present();
    }

    waitForCanvasLastPresentComplete(canvas.get());
    CHECK(true);
  }
#endif
}

TEST_CASE("MetalCanvas applies rounded clip masks to child content") {
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

    auto root = std::make_unique<GroupNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
    root->appendChild(std::make_unique<RectNode>(
        flux::Rect{0.f, 0.f, 640.f, 480.f},
        FillStyle::solid(Colors::white)
    ));

    auto clip = std::make_unique<RectNode>(
        flux::Rect{20.f, 20.f, 80.f, 20.f},
        FillStyle::none(),
        StrokeStyle::none(),
        CornerRadius::pill(flux::Rect::sharp(0.f, 0.f, 80.f, 20.f))
    );
    clip->setClipsContents(true);
    clip->appendChild(std::make_unique<RectNode>(
        flux::Rect{0.f, 0.f, 80.f, 20.f},
        FillStyle::solid(Colors::red)
    ));
    root->appendChild(std::move(clip));

    SceneGraph graph{std::move(root)};
    SceneRenderer renderer{*canvas};

    requestNextFrameCaptureForCanvas(canvas.get());
    canvas->beginFrame();
    canvas->clear(Colors::white);
    renderer.render(graph);
    canvas->present();
    waitForCanvasLastPresentComplete(canvas.get());

    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    REQUIRE(takeCapturedFrameForCanvas(canvas.get(), pixels, width, height));
    REQUIRE(width >= 200);
    REQUIRE(height >= 120);

    std::uint32_t const outsideX = 42;
    std::uint32_t const outsideY = 42;
    CHECK(capturedChannel(pixels, width, outsideX, outsideY, 0) >= 240);
    CHECK(capturedChannel(pixels, width, outsideX, outsideY, 1) >= 240);
    CHECK(capturedChannel(pixels, width, outsideX, outsideY, 2) >= 240);

    std::uint32_t const insideX = 120;
    std::uint32_t const insideY = 60;
    CHECK(capturedChannel(pixels, width, insideX, insideY, 2) >= 180);
    CHECK(capturedChannel(pixels, width, insideX, insideY, 1) <= 80);
    CHECK(capturedChannel(pixels, width, insideX, insideY, 0) <= 80);
  }
#endif
}
