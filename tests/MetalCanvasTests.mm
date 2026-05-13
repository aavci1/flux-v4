#include <doctest/doctest.h>

#include "Graphics/CoreTextSystem.hpp"
#include "Graphics/Metal/MetalCanvas.hpp"
#include "Graphics/Metal/MetalFrameRecorder.hpp"

#include <Flux/Graphics/Image.hpp>
#include <Flux/SceneGraph/SceneNode.hpp>
#include <Flux/SceneGraph/ImageNode.hpp>
#include <Flux/SceneGraph/PathNode.hpp>
#include <Flux/SceneGraph/RasterCacheNode.hpp>
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
  path /= "../examples/image-demo/test.webp";
  return std::filesystem::weakly_canonical(path);
}

static std::shared_ptr<TextLayout const> makeLabel(CoreTextSystem& textSystem, std::string const& text) {
  Font font{};
  font.family = ".AppleSystemUIFont";
  font.size = 13.f;
  font.weight = 400.f;
  return textSystem.layout(text, font, Colors::white, 120.f, {});
}

static MetalRecorderSlice fullSlice(MetalFrameRecorder const& recorded) {
  return MetalRecorderSlice{
      .orderStart = 0,
      .orderCount = static_cast<std::uint32_t>(recorded.opOrder.size()),
      .rectStart = 0,
      .rectCount = static_cast<std::uint32_t>(recorded.rectOps.size()),
      .imageStart = 0,
      .imageCount = static_cast<std::uint32_t>(recorded.imageOps.size()),
      .pathOpStart = 0,
      .pathOpCount = static_cast<std::uint32_t>(recorded.pathOps.size()),
      .glyphOpStart = 0,
      .glyphOpCount = static_cast<std::uint32_t>(recorded.glyphOps.size()),
      .backdropBlurOpStart = 0,
      .backdropBlurOpCount = static_cast<std::uint32_t>(recorded.backdropBlurOps.size()),
      .pathVertexStart = 0,
      .pathVertexCount = static_cast<std::uint32_t>(recorded.pathVerts.size()),
      .glyphVertexStart = 0,
      .glyphVertexCount = recorded.glyphVertexCount,
  };
}

struct StressScene {
  std::unique_ptr<SceneGraph> graph;
  SceneNode* animatedGroup = nullptr;
};

static StressScene makeStressScene(CoreTextSystem& textSystem, std::shared_ptr<Image> const& image) {
  auto graph = std::make_unique<SceneGraph>();
  auto root = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
  root->appendChild(std::make_unique<RectNode>(
      flux::Rect{0.f, 0.f, 640.f, 480.f},
      FillStyle::solid(Color{0.08f, 0.09f, 0.11f, 1.f})
  ));

  auto animatedGroup = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
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

static int colorDelta(std::vector<std::uint8_t> const& pixels, std::uint32_t width,
                      std::uint32_t ax, std::uint32_t ay, std::uint32_t bx, std::uint32_t by) {
  int delta = 0;
  for (int channel = 0; channel < 3; ++channel) {
    delta += std::abs(static_cast<int>(capturedChannel(pixels, width, ax, ay, channel)) -
                      static_cast<int>(capturedChannel(pixels, width, bx, by, channel)));
  }
  return delta;
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

TEST_CASE("MetalCanvas rejects prepared glyph replay after atlas growth") {
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

    Font cachedFont{};
    cachedFont.family = ".AppleSystemUIFont";
    cachedFont.size = 24.f;
    cachedFont.weight = 500.f;
    auto cachedLayout = textSystem.layout("Cached", cachedFont, Colors::white, 160.f, {});

    MetalFrameRecorder recorded;
    canvas->beginFrame();
    canvas->clear(Colors::black);
    REQUIRE(beginRecordedOpsCaptureForCanvas(canvas.get(), &recorded));
    canvas->drawTextLayout(*cachedLayout, flux::Point{12.f, 12.f});
    endRecordedOpsCaptureForCanvas(canvas.get());
    canvas->present();
    waitForCanvasLastPresentComplete(canvas.get());

    REQUIRE(recorded.glyphVertexCount > 0);
    REQUIRE(recorded.glyphAtlasGeneration > 0);

    Font largeFont = cachedFont;
    largeFont.size = 240.f;
    auto largeLayout =
        textSystem.layout("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789", largeFont, Colors::white, 12000.f, {});

    canvas->beginFrame();
    canvas->clear(Colors::black);
    canvas->drawTextLayout(*largeLayout, flux::Point{0.f, 260.f});
    canvas->present();
    waitForCanvasLastPresentComplete(canvas.get());

    canvas->beginFrame();
    canvas->clear(Colors::black);
    CHECK_FALSE(replayRecordedLocalOpsForCanvas(canvas.get(), recorded, fullSlice(recorded)));
    canvas->present();
    waitForCanvasLastPresentComplete(canvas.get());
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

    auto root = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
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

TEST_CASE("MetalCanvas shades linear gradient rect fills") {
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

    auto root = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
    root->appendChild(std::make_unique<RectNode>(
        flux::Rect{0.f, 0.f, 640.f, 480.f},
        FillStyle::solid(Colors::black)
    ));
    root->appendChild(std::make_unique<RectNode>(
        flux::Rect{20.f, 20.f, 100.f, 40.f},
        FillStyle::linearGradient(Colors::red, Colors::blue, flux::Point{0.f, 0.f}, flux::Point{1.f, 0.f})
    ));

    SceneGraph graph{std::move(root)};
    SceneRenderer renderer{*canvas};

    requestNextFrameCaptureForCanvas(canvas.get());
    canvas->beginFrame();
    canvas->clear(Colors::black);
    renderer.render(graph);
    canvas->present();
    waitForCanvasLastPresentComplete(canvas.get());

    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    REQUIRE(takeCapturedFrameForCanvas(canvas.get(), pixels, width, height));

    std::uint32_t const leftX = 60;
    std::uint32_t const rightX = 220;
    std::uint32_t const y = 60;
    CHECK(capturedChannel(pixels, width, leftX, y, 2) > capturedChannel(pixels, width, leftX, y, 0) + 80);
    CHECK(capturedChannel(pixels, width, rightX, y, 0) > capturedChannel(pixels, width, rightX, y, 2) + 80);
  }
#endif
}

TEST_CASE("MetalCanvas shades radial and conical gradient rect fills") {
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

    auto root = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
    root->appendChild(std::make_unique<RectNode>(
        flux::Rect{0.f, 0.f, 640.f, 480.f},
        FillStyle::solid(Colors::black)
    ));
    root->appendChild(std::make_unique<RectNode>(
        flux::Rect{20.f, 80.f, 100.f, 100.f},
        FillStyle::radialGradient(Colors::white, Colors::black, flux::Point{0.5f, 0.5f}, 0.5f)
    ));
    root->appendChild(std::make_unique<RectNode>(
        flux::Rect{150.f, 80.f, 100.f, 100.f},
        FillStyle::conicalGradient({
            GradientStop{0.00f, Colors::red},
            GradientStop{0.33f, Colors::green},
            GradientStop{0.66f, Colors::blue},
            GradientStop{1.00f, Colors::red},
        })
    ));

    SceneGraph graph{std::move(root)};
    SceneRenderer renderer{*canvas};

    requestNextFrameCaptureForCanvas(canvas.get());
    canvas->beginFrame();
    canvas->clear(Colors::black);
    renderer.render(graph);
    canvas->present();
    waitForCanvasLastPresentComplete(canvas.get());

    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    REQUIRE(takeCapturedFrameForCanvas(canvas.get(), pixels, width, height));

    std::uint32_t const radialCenterX = 140;
    std::uint32_t const radialCenterY = 260;
    std::uint32_t const radialEdgeX = 50;
    CHECK(capturedChannel(pixels, width, radialCenterX, radialCenterY, 0) >
          capturedChannel(pixels, width, radialEdgeX, radialCenterY, 0) + 80);
    CHECK(capturedChannel(pixels, width, radialCenterX, radialCenterY, 1) >
          capturedChannel(pixels, width, radialEdgeX, radialCenterY, 1) + 80);
    CHECK(capturedChannel(pixels, width, radialCenterX, radialCenterY, 2) >
          capturedChannel(pixels, width, radialEdgeX, radialCenterY, 2) + 80);

    std::uint32_t const conicRightX = 490;
    std::uint32_t const conicLeftX = 310;
    std::uint32_t const conicY = 260;
    CHECK(colorDelta(pixels, width, conicRightX, conicY, conicLeftX, conicY) > 120);
  }
#endif
}

TEST_CASE("MetalCanvas preserves rounded rect geometry when clipped by the viewport") {
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

    auto root = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
    root->appendChild(std::make_unique<RectNode>(
        flux::Rect{0.f, 0.f, 640.f, 480.f},
        FillStyle::solid(Colors::white)
    ));

    auto clip = std::make_unique<RectNode>(
        flux::Rect{20.f, 30.f, 140.f, 120.f},
        FillStyle::none(),
        StrokeStyle::none(),
        CornerRadius{}
    );
    clip->setClipsContents(true);
    clip->appendChild(std::make_unique<RectNode>(
        flux::Rect{0.f, -10.f, 100.f, 80.f},
        FillStyle::solid(Colors::red),
        StrokeStyle::none(),
        CornerRadius{28.f, 28.f, 28.f, 28.f}
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

    std::uint32_t const curvedX = 42;
    std::uint32_t const curvedY = 68;
    CHECK(capturedChannel(pixels, width, curvedX, curvedY, 0) >= 240);
    CHECK(capturedChannel(pixels, width, curvedX, curvedY, 1) >= 240);
    CHECK(capturedChannel(pixels, width, curvedX, curvedY, 2) >= 240);

    std::uint32_t const interiorX = 56;
    std::uint32_t const interiorY = 76;
    CHECK(capturedChannel(pixels, width, interiorX, interiorY, 2) >= 180);
    CHECK(capturedChannel(pixels, width, interiorX, interiorY, 1) <= 80);
    CHECK(capturedChannel(pixels, width, interiorX, interiorY, 0) <= 80);
  }
#endif
}

TEST_CASE("MetalCanvas preserves image sampling when clipped by the viewport") {
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
    REQUIRE(image);

    auto root = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 640.f, 480.f});
    root->appendChild(std::make_unique<RectNode>(
        flux::Rect{0.f, 0.f, 640.f, 480.f},
        FillStyle::solid(Colors::white)
    ));
    root->appendChild(std::make_unique<ImageNode>(
        flux::Rect{20.f, 20.f, 120.f, 160.f},
        image,
        ImageFillMode::Stretch
    ));

    auto clip = std::make_unique<RectNode>(
        flux::Rect{180.f, 40.f, 120.f, 140.f},
        FillStyle::none(),
        StrokeStyle::none(),
        CornerRadius{}
    );
    clip->setClipsContents(true);
    clip->appendChild(std::make_unique<ImageNode>(
        flux::Rect{0.f, -20.f, 120.f, 160.f},
        image,
        ImageFillMode::Stretch
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

    std::uint32_t const leftX = 100;
    std::uint32_t const leftY = 120;
    std::uint32_t const rightX = 420;
    std::uint32_t const rightY = 120;
    CHECK(colorDelta(pixels, width, leftX, leftY, rightX, rightY) <= 18);
  }
#endif
}

TEST_CASE("SceneRenderer rasterizes RasterCacheNode into a reusable Metal image") {
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

    auto root = std::make_unique<SceneNode>(flux::Rect{0.f, 0.f, 160.f, 120.f});
    auto raster = std::make_unique<RasterCacheNode>(flux::Rect{20.f, 24.f, 80.f, 40.f});
    RasterCacheNode* rasterNode = raster.get();
    raster->setSubtree(std::make_unique<RectNode>(
        flux::Rect{0.f, 0.f, 80.f, 40.f},
        FillStyle::solid(Colors::red)
    ));
    root->appendChild(std::move(raster));

    SceneGraph graph{std::move(root)};
    SceneRenderer renderer{*canvas};

    canvas->beginFrame();
    canvas->clear(Colors::black);
    renderer.render(graph);
    std::shared_ptr<Image> firstCache = rasterNode->cachedImage();
    REQUIRE(firstCache);
    CHECK(firstCache->size() == flux::Size{160.f, 80.f});
    canvas->present();
    waitForCanvasLastPresentComplete(canvas.get());

    canvas->beginFrame();
    canvas->clear(Colors::black);
    renderer.render(graph);
    CHECK(rasterNode->cachedImage() == firstCache);
    canvas->present();
    waitForCanvasLastPresentComplete(canvas.get());
  }
#endif
}
