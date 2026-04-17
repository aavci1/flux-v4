// Metal paint pipeline benchmark (macOS only).
// Build: cmake -B build -DFLUX_BUILD_BENCHMARKS=ON && cmake --build build --target paint_bench

#include "Graphics/CoreTextSystem.hpp"
#include "Graphics/Metal/MetalCanvas.hpp"

#include <Flux/Graphics/Image.hpp>
#include <Flux/Scene/SceneGraph.hpp>
#include <Flux/Scene/SceneRenderer.hpp>

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/QuartzCore.h>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kCanvasWidth = 1920;
constexpr int kCanvasHeight = 1080;
constexpr float kCanvasScale = 2.f;
constexpr int kRectCount = 5000;
constexpr int kTextCount = 5000;
constexpr int kMixedRectCount = 1000;
constexpr int kMixedTextCount = 500;
constexpr int kMixedImageCount = 100;
constexpr int kScrollRectCount = 1000;
constexpr int kArenaCounts[] = {512, 1536, 3072, 5000, 2048, 4096};
constexpr int kSteadyIterations = 60;
constexpr int kTextIterations = 40;
constexpr int kArenaIterations = 36;

struct BenchSurface {
  NSWindow* window = nil;
  NSView* view = nil;
  CAMetalLayer* layer = nil;
};

struct BenchmarkEnv {
  flux::CoreTextSystem textSystem;
  BenchSurface surface;
  std::unique_ptr<flux::Canvas> canvas;
  std::shared_ptr<flux::Image> image;
  flux::Font font{};
  flux::TextLayoutOptions textOptions{};
};

struct FrameTiming {
  double cpuSeconds = 0.0;
  double gpuSeconds = 0.0;
};

struct CaseMetrics {
  std::string name;
  int iterations = 0;
  double cpuAvgUs = 0.0;
  double gpuAvgUs = 0.0;
};

struct LayeredRects {
  flux::SceneGraph graph;
  std::vector<flux::NodeId> rectIds;
  std::vector<flux::Rect> baseBounds;
};

struct FlatRects {
  flux::SceneGraph graph;
};

struct LayeredTexts {
  flux::SceneGraph graph;
  std::vector<flux::NodeId> textIds;
  std::vector<std::string> labels;
};

struct FlatTexts {
  flux::SceneGraph graph;
};

struct MixedScene {
  flux::SceneGraph graph;
};

static flux::Color paletteColor(int i, float alpha = 1.f) {
  float const r = static_cast<float>((37 * i + 17) % 255) / 255.f;
  float const g = static_cast<float>((91 * i + 53) % 255) / 255.f;
  float const b = static_cast<float>((17 * i + 191) % 255) / 255.f;
  return flux::Color{r, g, b, alpha};
}

static BenchSurface makeSurface() {
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];

  NSRect const frame = NSMakeRect(-10000.f, -10000.f, static_cast<CGFloat>(kCanvasWidth),
                                  static_cast<CGFloat>(kCanvasHeight));
  BenchSurface surface;
  surface.window = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:NSWindowStyleMaskBorderless
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
  surface.window.opaque = YES;
  surface.window.backgroundColor = NSColor.blackColor;
  surface.view = [[NSView alloc] initWithFrame:NSMakeRect(0.f, 0.f, frame.size.width, frame.size.height)];
  surface.view.wantsLayer = YES;

  surface.layer = [CAMetalLayer layer];
  surface.layer.device = MTLCreateSystemDefaultDevice();
  surface.layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  surface.layer.contentsScale = kCanvasScale;
  surface.layer.maximumDrawableCount = 3;
  surface.layer.allowsNextDrawableTimeout = YES;
  surface.layer.displaySyncEnabled = NO;
  surface.layer.frame = surface.view.bounds;
  surface.layer.drawableSize =
      CGSizeMake(frame.size.width * kCanvasScale, frame.size.height * kCanvasScale);
  surface.layer.opaque = YES;
  surface.layer.needsDisplayOnBoundsChange = YES;
  surface.view.layer = surface.layer;

  [surface.window setContentView:surface.view];
  [surface.window orderFrontRegardless];
  [surface.window displayIfNeeded];
  [surface.view displayIfNeeded];
  [CATransaction flush];
  return surface;
}

static std::filesystem::path imageFixturePath() {
  std::filesystem::path path = std::filesystem::path(__FILE__).parent_path();
  path /= "../../examples/image-demo/test.png";
  return std::filesystem::weakly_canonical(path);
}

static std::unique_ptr<BenchmarkEnv> makeEnv() {
  auto env = std::make_unique<BenchmarkEnv>();
  env->surface = makeSurface();
  env->font.family = ".AppleSystemUIFont";
  env->font.size = 13.f;
  env->font.weight = 400.f;
  env->canvas = flux::createMetalCanvas(nullptr, (__bridge void*)env->surface.layer, 0, env->textSystem);
  env->canvas->resize(kCanvasWidth, kCanvasHeight);
  env->canvas->updateDpiScale(kCanvasScale, kCanvasScale);
  env->image = flux::loadImageFromFile(imageFixturePath().string(), env->canvas->gpuDevice());
  return env;
}

static std::shared_ptr<flux::TextLayout const> makeLabelLayout(BenchmarkEnv& env, std::string_view text,
                                                               flux::Color color, float width = 120.f) {
  return env.textSystem.layout(text, env.font, color, width, env.textOptions);
}

static FrameTiming renderFrame(flux::SceneRenderer& renderer, flux::SceneGraph const& graph, flux::Canvas& canvas,
                               bool syncPresent) {
  flux::setSyncPresentForCanvas(&canvas, syncPresent);
  auto const t0 = Clock::now();
  canvas.beginFrame();
  renderer.render(graph, canvas, flux::Color{0.08f, 0.09f, 0.11f, 1.f});
  canvas.present();
  auto const t1 = Clock::now();
  flux::waitForCanvasLastPresentComplete(&canvas);
  auto const t2 = Clock::now();
  return FrameTiming{
      .cpuSeconds = std::chrono::duration<double>(t1 - t0).count(),
      .gpuSeconds = std::chrono::duration<double>(t2 - t0).count(),
  };
}

static void primeScene(flux::SceneRenderer& renderer, flux::SceneGraph const& graph, flux::Canvas& canvas) {
  (void)renderFrame(renderer, graph, canvas, false);
}

static std::vector<std::uint8_t> captureFrame(flux::SceneRenderer& renderer, flux::SceneGraph const& graph,
                                              flux::Canvas& canvas, std::uint32_t& width,
                                              std::uint32_t& height) {
  (void)flux::requestNextFrameCaptureForCanvas(&canvas);
  (void)renderFrame(renderer, graph, canvas, false);
  std::vector<std::uint8_t> pixels;
  width = 0;
  height = 0;
  (void)flux::takeCapturedFrameForCanvas(&canvas, pixels, width, height);
  return pixels;
}

static CaseMetrics averageCase(std::string name, int iterations, auto&& fn) {
  double cpu = 0.0;
  double gpu = 0.0;
  for (int i = 0; i < iterations; ++i) {
    FrameTiming const timing = fn(i);
    cpu += timing.cpuSeconds;
    gpu += timing.gpuSeconds;
  }
  return CaseMetrics{
      .name = std::move(name),
      .iterations = iterations,
      .cpuAvgUs = cpu * 1e6 / static_cast<double>(iterations),
      .gpuAvgUs = gpu * 1e6 / static_cast<double>(iterations),
  };
}

static flux::Rect rectGridBounds(int index, int cols, float cellW, float cellH, float padX, float padY) {
  int const col = index % cols;
  int const row = index / cols;
  return flux::Rect::sharp(col * cellW + padX, row * cellH + padY, cellW - padX * 2.f, cellH - padY * 2.f);
}

static LayeredRects makeRectScene(int count) {
  LayeredRects scene;
  scene.rectIds.reserve(static_cast<std::size_t>(count));
  scene.baseBounds.reserve(static_cast<std::size_t>(count));
  constexpr int cols = 100;
  constexpr float cellW = 18.f;
  constexpr float cellH = 18.f;
  for (int i = 0; i < count; ++i) {
    flux::NodeId const layerId = scene.graph.addLayer(scene.graph.root(), flux::LayerNode{});
    flux::Rect const bounds = rectGridBounds(i, cols, cellW, cellH, 2.f, 2.f);
    flux::NodeId const rectId =
        scene.graph.addRect(layerId, flux::RectNode{
                                         .bounds = bounds,
                                         .cornerRadius = flux::CornerRadius{3.f, 3.f, 3.f, 3.f},
                                         .fill = flux::FillStyle::solid(paletteColor(i)),
                                         .stroke = flux::StrokeStyle::none(),
                                         .shadow = flux::ShadowStyle::none(),
                                     });
    scene.rectIds.push_back(rectId);
    scene.baseBounds.push_back(bounds);
  }
  return scene;
}

static FlatRects makeFlatRectScene(int count) {
  FlatRects scene;
  constexpr int cols = 100;
  constexpr float cellW = 18.f;
  constexpr float cellH = 18.f;
  for (int i = 0; i < count; ++i) {
    flux::Rect const bounds = rectGridBounds(i, cols, cellW, cellH, 2.f, 2.f);
    scene.graph.addRect(scene.graph.root(), flux::RectNode{
                                              .bounds = bounds,
                                              .cornerRadius = flux::CornerRadius{3.f, 3.f, 3.f, 3.f},
                                              .fill = flux::FillStyle::solid(paletteColor(i)),
                                              .stroke = flux::StrokeStyle::none(),
                                              .shadow = flux::ShadowStyle::none(),
                                          });
  }
  return scene;
}

static LayeredTexts makeTextScene(BenchmarkEnv& env, int count) {
  LayeredTexts scene;
  scene.textIds.reserve(static_cast<std::size_t>(count));
  scene.labels.reserve(static_cast<std::size_t>(count));
  constexpr int cols = 100;
  constexpr float cellW = 18.f;
  constexpr float cellH = 18.f;
  for (int i = 0; i < count; ++i) {
    flux::NodeId const layerId = scene.graph.addLayer(scene.graph.root(), flux::LayerNode{});
    flux::Rect const bounds = rectGridBounds(i, cols, cellW, cellH, 1.f, 1.f);
    scene.labels.push_back("Label " + std::to_string(i));
    flux::NodeId const textId =
        scene.graph.addText(layerId, flux::TextNode{
                                         .layout = makeLabelLayout(env, scene.labels.back(), paletteColor(i), 80.f),
                                         .origin = flux::Point{bounds.x, bounds.y + 12.f},
                                         .allocation = flux::Rect::sharp(bounds.x, bounds.y, 80.f, 14.f),
                                     });
    scene.textIds.push_back(textId);
  }
  return scene;
}

static FlatTexts makeFlatTextScene(BenchmarkEnv& env, int count) {
  FlatTexts scene;
  constexpr int cols = 100;
  constexpr float cellW = 18.f;
  constexpr float cellH = 18.f;
  for (int i = 0; i < count; ++i) {
    flux::Rect const bounds = rectGridBounds(i, cols, cellW, cellH, 1.f, 1.f);
    scene.graph.addText(scene.graph.root(), flux::TextNode{
                                               .layout = makeLabelLayout(env, "Label " + std::to_string(i),
                                                                         paletteColor(i), 80.f),
                                               .origin = flux::Point{bounds.x, bounds.y + 12.f},
                                               .allocation = flux::Rect::sharp(bounds.x, bounds.y, 80.f, 14.f),
                                           });
  }
  return scene;
}

static MixedScene makeMixedScene(BenchmarkEnv& env) {
  MixedScene scene;
  for (int i = 0; i < kMixedRectCount; ++i) {
    flux::Rect const bounds = rectGridBounds(i, 40, 40.f, 28.f, 3.f, 3.f);
    scene.graph.addRect(scene.graph.root(), flux::RectNode{
                                               .bounds = bounds,
                                               .cornerRadius = flux::CornerRadius{4.f, 4.f, 4.f, 4.f},
                                               .fill = flux::FillStyle::solid(paletteColor(i)),
                                               .stroke = flux::StrokeStyle::none(),
                                               .shadow = flux::ShadowStyle::none(),
                                           });
  }

  for (int i = 0; i < kMixedTextCount; ++i) {
    float const x = static_cast<float>((i % 25) * 74);
    float const y = 720.f + static_cast<float>(i / 25) * 16.f;
    scene.graph.addText(scene.graph.root(), flux::TextNode{
                                              .layout = makeLabelLayout(env, "Row " + std::to_string(i),
                                                                        flux::Color{0.95f, 0.96f, 0.98f, 1.f},
                                                                        64.f),
                                              .origin = flux::Point{x, y},
                                              .allocation = flux::Rect::sharp(x, y - 12.f, 64.f, 14.f),
                                          });
  }

  if (env.image) {
    for (int i = 0; i < kMixedImageCount; ++i) {
      float const x = static_cast<float>((i % 10) * 90);
      float const y = 880.f + static_cast<float>(i / 10) * 90.f;
      scene.graph.addImage(scene.graph.root(), flux::ImageNode{
                                                 .image = env.image,
                                                 .bounds = flux::Rect::sharp(x, y, 72.f, 72.f),
                                                 .fillMode = flux::ImageFillMode::Cover,
                                                 .cornerRadius = flux::CornerRadius{8.f, 8.f, 8.f, 8.f},
                                                 .opacity = 1.f,
                                             });
    }
  }

  return scene;
}

static void mutateRect(LayeredRects& scene, int iteration) {
  int const index = iteration % static_cast<int>(scene.rectIds.size());
  if (auto* rect = scene.graph.node<flux::RectNode>(scene.rectIds[static_cast<std::size_t>(index)])) {
    flux::Rect const base = scene.baseBounds[static_cast<std::size_t>(index)];
    float const delta = (iteration & 1) ? 1.f : -1.f;
    rect->bounds = flux::Rect::sharp(base.x + delta, base.y, base.width, base.height);
    scene.graph.markPaintDirty(rect->id);
  }
}

static void mutateText(BenchmarkEnv& env, LayeredTexts& scene, int iteration) {
  int const index = iteration % static_cast<int>(scene.textIds.size());
  std::string& label = scene.labels[static_cast<std::size_t>(index)];
  label = "Label " + std::to_string(index) + ((iteration & 1) ? " x" : " y");
  if (auto* text = scene.graph.node<flux::TextNode>(scene.textIds[static_cast<std::size_t>(index)])) {
    text->layout = makeLabelLayout(env, label, paletteColor(index), 80.f);
    scene.graph.markPaintDirty(text->id);
  }
}

static void advanceRootScroll(flux::SceneGraph& graph, int iteration) {
  if (auto* root = graph.node<flux::LayerNode>(graph.root())) {
    root->transform = flux::Mat3::translate(0.f, static_cast<float>(iteration + 1));
    graph.markPaintDirty(root->id);
  }
}

static void printCase(CaseMetrics const& metrics) {
  std::cout << std::left << std::setw(28) << metrics.name << " cpu=" << std::right << std::setw(9)
            << std::fixed << std::setprecision(2) << metrics.cpuAvgUs << " us"
            << "  gpu=" << std::setw(9) << metrics.gpuAvgUs << " us"
            << "  iters=" << metrics.iterations << "\n";
}

static void printPixelCheck(std::string_view name, bool pass, std::uint32_t width, std::uint32_t height) {
  std::cout << std::left << std::setw(28) << std::string(name) << (pass ? "PASS" : "FAIL") << "  "
            << width << "x" << height << "\n";
}

} // namespace

int main() {
#if !defined(__APPLE__)
  std::cerr << "paint_bench is only supported on Apple platforms.\n";
  return 0;
#else
  @autoreleasepool {
    using namespace flux;

    std::unique_ptr<BenchmarkEnv> env = makeEnv();
    SceneRenderer renderer;

    std::cout << "paint_bench " << kCanvasWidth << "x" << kCanvasHeight << " @" << kCanvasScale << "x\n";
    std::cout << "cases report average CPU-only and GPU-inclusive frame time.\n\n";

    {
      FlatRects scene = makeFlatRectScene(kRectCount);
      printCase(averageCase("P1 cold", 1, [&](int) {
        return renderFrame(renderer, scene.graph, *env->canvas, false);
      }));
    }

    {
      FlatRects scene = makeFlatRectScene(kRectCount);
      primeScene(renderer, scene.graph, *env->canvas);
      printCase(averageCase("P2 steady unchanged", kSteadyIterations, [&](int) {
        return renderFrame(renderer, scene.graph, *env->canvas, false);
      }));
    }

    {
      LayeredRects scene = makeRectScene(kRectCount);
      primeScene(renderer, scene.graph, *env->canvas);
      printCase(averageCase("P3 steady 1-rect-mutation", kSteadyIterations, [&](int i) {
        mutateRect(scene, i);
        return renderFrame(renderer, scene.graph, *env->canvas, false);
      }));
    }

    {
      FlatTexts scene = makeFlatTextScene(*env, kTextCount);
      primeScene(renderer, scene.graph, *env->canvas);
      printCase(averageCase("P4 5000 Text unchanged", kTextIterations, [&](int) {
        return renderFrame(renderer, scene.graph, *env->canvas, false);
      }));
    }

    {
      LayeredTexts scene = makeTextScene(*env, kTextCount);
      primeScene(renderer, scene.graph, *env->canvas);
      printCase(averageCase("P5 5000 Text 1-row-mutation", kTextIterations, [&](int i) {
        mutateText(*env, scene, i);
        return renderFrame(renderer, scene.graph, *env->canvas, false);
      }));
    }

    {
      MixedScene scene = makeMixedScene(*env);
      primeScene(renderer, scene.graph, *env->canvas);
      printCase(averageCase("P6 mixed", kSteadyIterations, [&](int) {
        return renderFrame(renderer, scene.graph, *env->canvas, false);
      }));
    }

    {
      FlatRects scene = makeFlatRectScene(kScrollRectCount);
      primeScene(renderer, scene.graph, *env->canvas);
      printCase(averageCase("P7 scroll", kSteadyIterations, [&](int i) {
        advanceRootScroll(scene.graph, i);
        return renderFrame(renderer, scene.graph, *env->canvas, false);
      }));
    }

    {
      std::vector<FlatRects> scenes;
      for (int count : kArenaCounts) {
        scenes.push_back(makeFlatRectScene(count));
      }
      printCase(averageCase("P8 arena churn", kArenaIterations, [&](int i) {
        FlatRects& scene = scenes[static_cast<std::size_t>(i) % scenes.size()];
        return renderFrame(renderer, scene.graph, *env->canvas, false);
      }));
    }

    {
      FlatRects scene = makeFlatRectScene(kRectCount);
      primeScene(renderer, scene.graph, *env->canvas);
      printCase(averageCase("P9 synchronous present", kSteadyIterations, [&](int) {
        return renderFrame(renderer, scene.graph, *env->canvas, true);
      }));
    }

    {
      FlatRects rectScene = makeFlatRectScene(kRectCount);
      FlatTexts textScene = makeFlatTextScene(*env, kTextCount);
      MixedScene mixedScene = makeMixedScene(*env);

      auto runCheck = [&](std::string_view name, auto& scene) {
        std::uint32_t coldW = 0;
        std::uint32_t coldH = 0;
        std::vector<std::uint8_t> const cold = captureFrame(renderer, scene.graph, *env->canvas, coldW, coldH);
        std::uint32_t warmW = 0;
        std::uint32_t warmH = 0;
        std::vector<std::uint8_t> const warm = captureFrame(renderer, scene.graph, *env->canvas, warmW, warmH);
        bool const pass = !cold.empty() && coldW == warmW && coldH == warmH && cold == warm;
        printPixelCheck(std::string("P10 pixel ") + std::string(name), pass, warmW, warmH);
      };

      runCheck("rect", rectScene);
      runCheck("text", textScene);
      runCheck("mixed", mixedScene);
    }
  }
  return 0;
#endif
}
