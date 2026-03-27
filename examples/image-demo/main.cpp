#include <Flux.hpp>
#include <Flux/Graphics/Canvas.hpp>
#include <Flux/Graphics/Image.hpp>
#include <Flux/Graphics/ImageFillMode.hpp>
#include <Flux/Graphics/Styles.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>

using namespace flux;

namespace {

std::string defaultImagePath() {
  namespace fs = std::filesystem;
  return fs::absolute(fs::path(__FILE__).parent_path() / "test.png").string();
}

class ImageDemoWindow : public Window {
public:
  explicit ImageDemoWindow(WindowConfig const& c, std::string imagePath) : Window(c), imagePath_(std::move(imagePath)) {}

  void render(Canvas& c) override {
    c.clear(Color::rgb(245, 246, 250));

    Rect const vb = c.clipBounds();
    Size const sz = getSize();
    float const w = std::max({vb.width, sz.width, 1.f});
    float const h = std::max({vb.height, sz.height, 1.f});

    if (!image_ && !imageLoadAttempted_) {
      imageLoadAttempted_ = true;
      image_ = loadImageFromFile(imagePath_, c.gpuDevice());
    }

    if (!image_) {
      return;
    }

    float const pad = 0.f;
    float const bottomH = 56.f;
    float const colW = (w - pad * 3.f) * 0.5f;
    float const rowH = (h - pad * 3.f - bottomH) * 0.5f;

    auto cell = [&](float cx, float cy, ImageFillMode mode) {
      Rect const r{cx, cy, colW, rowH};
      c.drawImage(*image_, r, mode);
    };

    cell(pad, pad, ImageFillMode::Cover);
    cell(pad * 2.f + colW, pad, ImageFillMode::Fit);
    cell(pad, pad * 2.f + rowH, ImageFillMode::Stretch);
    cell(pad * 2.f + colW, pad * 2.f + rowH, ImageFillMode::Center);

    float const stripY = h - pad - bottomH;
    float const halfStrip = (w - pad * 3.f) * 0.5f;
    Rect const tileDst{pad, stripY, halfStrip, bottomH - 8.f};
    c.drawImage(*image_, tileDst, ImageFillMode::Tile);

    Size const is = image_->size();
    if (is.width > 1.f && is.height > 1.f) {
      Rect const srcDst{pad * 2.f + halfStrip, stripY, halfStrip, bottomH - 8.f};
      Rect const src{0.f, 0.f, is.width * 0.5f, is.height};
      c.drawImage(*image_, src, srcDst);
    }
  }

private:
  std::string imagePath_;
  bool imageLoadAttempted_{false};
  std::shared_ptr<Image> image_;
};

} // namespace

int main(int argc, char* argv[]) {
  Application app(argc, argv);

  std::string const imagePath = argc > 1 ? std::string(argv[1]) : defaultImagePath();

  app.createWindow<ImageDemoWindow>(
      {
          .size = {640, 520},
          .title = "Flux · drawImage",
      },
      imagePath);

  return app.exec();
}
