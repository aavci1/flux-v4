#include <doctest/doctest.h>

#include "Graphics/CoreTextSystem.hpp"
#include "Graphics/Metal/MetalCanvas.hpp"

#include <Flux/Graphics/Image.hpp>
#include <Flux/Scene/ImageSceneNode.hpp>
#include <Flux/Scene/PathSceneNode.hpp>
#include <Flux/Scene/RectSceneNode.hpp>
#include <Flux/Scene/SceneTree.hpp>
#include <Flux/Scene/TextSceneNode.hpp>

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
  SceneTree tree{};
  SceneNode* animatedGroup = nullptr;
};

static StressScene makeStressScene(CoreTextSystem& textSystem, std::shared_ptr<Image> const& image) {
  auto root = std::make_unique<SceneNode>(NodeId{1ull});

  auto bg = std::make_unique<RectSceneNode>(NodeId{2ull});
  bg->size = flux::Size{640.f, 480.f};
  bg->fill = FillStyle::solid(Color{0.08f, 0.09f, 0.11f, 1.f});
  bg->recomputeBounds();
  root->appendChild(std::move(bg));

  auto animatedGroup = std::make_unique<SceneNode>(NodeId{3ull});
  SceneNode* animatedGroupPtr = animatedGroup.get();

  for (int i = 0; i < 256; ++i) {
    auto rect = std::make_unique<RectSceneNode>(NodeId{100ull + static_cast<std::uint64_t>(i)});
    rect->position = flux::Point{
        static_cast<float>((i % 32) * 18),
        static_cast<float>((i / 32) * 18),
    };
    rect->size = flux::Size{14.f, 14.f};
    rect->cornerRadius = CornerRadius{3.f, 3.f, 3.f, 3.f};
    rect->fill = FillStyle::solid(Color{
        static_cast<float>((17 * i) % 255) / 255.f,
        static_cast<float>((37 * i) % 255) / 255.f,
        static_cast<float>((53 * i) % 255) / 255.f,
        1.f,
    });
    rect->recomputeBounds();
    animatedGroup->appendChild(std::move(rect));
  }

  for (int i = 0; i < 64; ++i) {
    auto text = std::make_unique<TextSceneNode>(NodeId{1000ull + static_cast<std::uint64_t>(i)});
    text->layout = makeLabel(textSystem, "Row " + std::to_string(i));
    text->position = flux::Point{
        static_cast<float>((i % 8) * 72),
        170.f + static_cast<float>(i / 8) * 16.f,
    };
    text->origin = flux::Point{0.f, 0.f};
    text->allocation = flux::Rect::sharp(0.f, -12.f, 64.f, 14.f);
    text->recomputeBounds();
    animatedGroup->appendChild(std::move(text));
  }

  Path triangle;
  triangle.moveTo({320.f, 40.f});
  triangle.lineTo({420.f, 180.f});
  triangle.lineTo({240.f, 180.f});
  triangle.close();
  auto path = std::make_unique<PathSceneNode>(NodeId{2000ull});
  path->path = triangle;
  path->fill = FillStyle::solid(Color{0.2f, 0.6f, 0.9f, 1.f});
  path->stroke = StrokeStyle::none();
  path->shadow = ShadowStyle::none();
  path->recomputeBounds();
  animatedGroup->appendChild(std::move(path));

  if (image) {
    for (int i = 0; i < 9; ++i) {
      auto imageNode = std::make_unique<ImageSceneNode>(NodeId{3000ull + static_cast<std::uint64_t>(i)});
      imageNode->image = image;
      imageNode->position = flux::Point{
          static_cast<float>((i % 3) * 88),
          320.f + static_cast<float>(i / 3) * 88.f,
      };
      imageNode->size = flux::Size{72.f, 72.f};
      imageNode->fillMode = ImageFillMode::Cover;
      imageNode->cornerRadius = CornerRadius{6.f, 6.f, 6.f, 6.f};
      imageNode->opacity = 1.f;
      imageNode->recomputeBounds();
      animatedGroup->appendChild(std::move(imageNode));
    }
  }

  animatedGroup->recomputeBounds();
  root->appendChild(std::move(animatedGroup));
  root->recomputeBounds();
  return StressScene{SceneTree{std::move(root)}, animatedGroupPtr};
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

    for (int frame = 0; frame < 18; ++frame) {
      scene.animatedGroup->position = flux::Point{0.f, static_cast<float>(frame % 3)};
      scene.tree.root().recomputeBounds();
      canvas->beginFrame();
      canvas->clear(Color{0.08f, 0.09f, 0.11f, 1.f});
      render(scene.tree, *canvas);
      canvas->present();
    }

    waitForCanvasLastPresentComplete(canvas.get());
    CHECK(true);
  }
#endif
}
