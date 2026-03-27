#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>

#include "Core/PlatformWindow.hpp"
#include "Core/PlatformWindowCreate.hpp"
#include "Graphics/Metal/MetalCanvas.hpp"

#include <algorithm>
#include <atomic>
#include <dispatch/dispatch.h>
#include <memory>
#include <string>

namespace flux {
class MacMetalPlatformWindow;
} // namespace flux

@interface FluxMetalView : NSView
@property(nonatomic, assign) flux::MacMetalPlatformWindow* fluxPlatform;
- (CAMetalLayer*)fluxMetalLayer;
- (void)updateDrawableSize;
@end

@implementation FluxMetalView

/// NSView may use `NSViewBackingLayer` unless we supply a Metal layer here.
/// `+layerClass` alone is not always reliable on newer macOS for `setDevice:`.
- (CALayer*)makeBackingLayer {
  CAMetalLayer* metalLayer = [CAMetalLayer layer];
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (device) {
    metalLayer.device = device;
  }
  metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  metalLayer.contentsScale = [NSScreen mainScreen].backingScaleFactor;
  // Match MetalCanvas in-flight limit; helps avoid main-thread stalls on nextDrawable during live resize.
  metalLayer.maximumDrawableCount = 3;
  metalLayer.allowsNextDrawableTimeout = YES;
  return metalLayer;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    self.wantsLayer = YES;
    [self updateDrawableSize];
  }
  return self;
}

- (CAMetalLayer*)fluxMetalLayer {
  CALayer* layer = self.layer;
  if ([layer isKindOfClass:[CAMetalLayer class]]) {
    return static_cast<CAMetalLayer*>(layer);
  }
  return nil;
}

- (CGFloat)fluxBackingScale {
  NSWindow* w = self.window;
  if (w) {
    return w.backingScaleFactor;
  }
  return [NSScreen mainScreen].backingScaleFactor;
}

- (void)layout {
  [super layout];
  [self updateDrawableSize];
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  CAMetalLayer* metalLayer = [self fluxMetalLayer];
  if (metalLayer && self.window) {
    metalLayer.contentsScale = self.window.backingScaleFactor;
  }
  [self updateDrawableSize];
  [self updateTrackingAreas];
}

- (void)viewDidChangeBackingProperties {
  [super viewDidChangeBackingProperties];
  CAMetalLayer* metalLayer = [self fluxMetalLayer];
  if (metalLayer && self.window) {
    metalLayer.contentsScale = self.window.backingScaleFactor;
  }
  [self updateDrawableSize];
}

- (void)updateDrawableSize {
  CAMetalLayer* metalLayer = [self fluxMetalLayer];
  if (!metalLayer) {
    return;
  }
  CGFloat scale = [self fluxBackingScale];
  CGSize bounds = self.bounds.size;
  CGFloat w = (std::max)(bounds.width * scale, static_cast<CGFloat>(1.0));
  CGFloat h = (std::max)(bounds.height * scale, static_cast<CGFloat>(1.0));
  metalLayer.drawableSize = CGSizeMake(w, h);
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)isFlipped {
  return YES;
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  for (NSTrackingArea* area in self.trackingAreas) {
    [self removeTrackingArea:area];
  }
  NSTrackingAreaOptions opts =
      NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow |
      NSTrackingInVisibleRect | NSTrackingCursorUpdate | NSTrackingEnabledDuringMouseDrag;
  NSTrackingArea* ta =
      [[NSTrackingArea alloc] initWithRect:self.bounds options:opts owner:self userInfo:nil];
  [self addTrackingArea:ta];
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  [self updateTrackingAreas];
}

@end

@interface FluxWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) flux::MacMetalPlatformWindow* platform;
@end

namespace flux {

namespace {

std::atomic<unsigned int> gNextHandle{1};

} // namespace

class MacMetalPlatformWindow : public PlatformWindow {
public:
  explicit MacMetalPlatformWindow(const WindowConfig& config);
  ~MacMetalPlatformWindow() override;

  void setFluxWindow(Window* window) override;
  void show() override;
  void resize(const Size& newSize) override;
  void setFullscreen(bool fullscreen) override;
  void setTitle(const std::string& title) override;
  Size currentSize() const override;
  bool isFullscreen() const override;
  unsigned int handle() const override;
  void* nativeGraphicsSurface() const override;

  std::unique_ptr<Canvas> createCanvas(Window& owner) override;

  void processEvents() override;
  void waitForEvents(int timeoutMs) override;
  void wakeEventLoop() override;

  Window* fluxWindow() const;

private:
  struct Impl;
  std::unique_ptr<Impl> d;
};

struct MacMetalPlatformWindow::Impl {
  NSWindow* window_{nil};
  FluxMetalView* metalView_{nil};
  FluxWindowDelegate* delegate_{nil};
  Window* fluxWindow_{nullptr};
  unsigned int handle_{0};
};

namespace detail {

Modifiers modifiersFromFlags(NSUInteger m) {
  Modifiers r = Modifiers::None;
  if (m & NSEventModifierFlagShift) {
    r = r | Modifiers::Shift;
  }
  if (m & NSEventModifierFlagControl) {
    r = r | Modifiers::Ctrl;
  }
  if (m & NSEventModifierFlagOption) {
    r = r | Modifiers::Alt;
  }
  if (m & NSEventModifierFlagCommand) {
    r = r | Modifiers::Meta;
  }
  return r;
}

Modifiers modifiersFromNSEvent(NSEvent* e) { return modifiersFromFlags(e.modifierFlags); }

MouseButton buttonFromNSEvent(NSEvent* e) {
  switch (e.buttonNumber) {
  case 0:
    return MouseButton::Left;
  case 1:
    return MouseButton::Right;
  case 2:
    return MouseButton::Middle;
  default:
    return MouseButton::Other;
  }
}

void postInputFromView(FluxMetalView* view, InputEvent::Kind kind, NSEvent* e, std::string text = {}) {
  MacMetalPlatformWindow* p = view.fluxPlatform;
  if (!p || !p->fluxWindow()) {
    return;
  }
  InputEvent ie;
  ie.kind = kind;
  ie.handle = p->fluxWindow()->handle();
  if (kind == InputEvent::Kind::Scroll) {
    ie.position =
        Vec2{static_cast<float>(e.scrollingDeltaX), static_cast<float>(e.scrollingDeltaY)};
  } else {
    NSPoint pt = [view convertPoint:[e locationInWindow] fromView:nil];
    ie.position = Vec2{static_cast<float>(pt.x), static_cast<float>(pt.y)};
  }
  ie.button = (kind == InputEvent::Kind::PointerMove || kind == InputEvent::Kind::Scroll)
                  ? MouseButton::None
                  : buttonFromNSEvent(e);
  ie.key = 0;
  if (kind == InputEvent::Kind::KeyDown || kind == InputEvent::Kind::KeyUp) {
    ie.key = static_cast<KeyCode>(e.keyCode);
  }
  ie.modifiers = modifiersFromNSEvent(e);
  ie.text = std::move(text);
  Application::instance().eventQueue().post(ie);
}

void postTextInput(FluxMetalView* view, std::string text) {
  MacMetalPlatformWindow* p = view.fluxPlatform;
  if (!p || !p->fluxWindow()) {
    return;
  }
  InputEvent ie;
  ie.kind = InputEvent::Kind::TextInput;
  ie.handle = p->fluxWindow()->handle();
  ie.modifiers = modifiersFromFlags([NSEvent modifierFlags]);
  ie.text = std::move(text);
  Application::instance().eventQueue().post(ie);
}

} // namespace detail

} // namespace flux

@implementation FluxWindowDelegate

- (void)windowWillClose:(NSNotification*)notification {
  (void)notification;
  flux::MacMetalPlatformWindow* platform = self.platform;
  if (!platform) {
    return;
  }
  flux::Window* w = platform->fluxWindow();
  if (!w) {
    return;
  }
  void (^block)(void) = ^{
    flux::Application::instance().eventQueue().post(flux::WindowEvent{flux::WindowEvent::Kind::CloseRequest,
                                                                        w->handle(), flux::Size{}, 1.0f});
  };
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_async(dispatch_get_main_queue(), block);
  }
}

- (void)windowDidResize:(NSNotification*)notification {
  flux::MacMetalPlatformWindow* platform = self.platform;
  if (!platform) {
    return;
  }
  flux::Window* fw = platform->fluxWindow();
  if (!fw) {
    return;
  }
  flux::Application::instance().eventQueue().post(flux::WindowEvent{flux::WindowEvent::Kind::Resize, fw->handle(),
                                                       platform->currentSize(), 1.0f});
  // Live resize runs in a tracking run loop mode where GCD timers / run-loop observers may not fire
  // until the drag ends; flush immediately so Resize → paint runs during the drag.
  flux::Application::instance().eventQueue().dispatch();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  (void)notification;
  flux::MacMetalPlatformWindow* platform = self.platform;
  if (!platform) {
    return;
  }
  flux::Window* fw = platform->fluxWindow();
  if (!fw) {
    return;
  }
  flux::Application::instance().eventQueue().post(
      flux::WindowEvent{flux::WindowEvent::Kind::FocusGained, fw->handle(), {}, 1.0f});
  flux::Application::instance().requestRedraw();
}

- (void)windowDidResignKey:(NSNotification*)notification {
  (void)notification;
  flux::MacMetalPlatformWindow* platform = self.platform;
  if (!platform) {
    return;
  }
  flux::Window* fw = platform->fluxWindow();
  if (!fw) {
    return;
  }
  flux::Application::instance().eventQueue().post(
      flux::WindowEvent{flux::WindowEvent::Kind::FocusLost, fw->handle(), {}, 1.0f});
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  NSWindow* win = static_cast<NSWindow*>(notification.object);
  flux::MacMetalPlatformWindow* platform = self.platform;
  if (!platform || !platform->fluxWindow()) {
    return;
  }
  CGFloat scale = win ? win.backingScaleFactor : 1.0;
  flux::Window* fw = platform->fluxWindow();
  flux::Application::instance().eventQueue().post(flux::WindowEvent{flux::WindowEvent::Kind::DpiChanged,
                                                       fw->handle(), {}, static_cast<float>(scale)});
}

@end

@implementation FluxMetalView (FluxInput)

- (void)mouseDown:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::PointerDown, event);
}

- (void)mouseUp:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::PointerUp, event);
}

- (void)mouseMoved:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::PointerMove, event);
}

- (void)mouseDragged:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::PointerMove, event);
}

- (void)rightMouseDown:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::PointerDown, event);
}

- (void)rightMouseUp:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::PointerUp, event);
}

- (void)otherMouseDown:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::PointerDown, event);
}

- (void)otherMouseUp:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::PointerUp, event);
}

- (void)scrollWheel:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::Scroll, event);
}

- (void)keyDown:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::KeyDown, event);
}

- (void)keyUp:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::KeyUp, event);
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  (void)replacementRange;
  NSString* s = (NSString*)string;
  std::string utf8 = s ? [s UTF8String] : "";
  flux::detail::postTextInput(self, std::move(utf8));
}

@end

namespace flux {

Window* MacMetalPlatformWindow::fluxWindow() const {
  return d ? d->fluxWindow_ : nullptr;
}

MacMetalPlatformWindow::MacMetalPlatformWindow(const WindowConfig& config) : d(std::make_unique<Impl>()) {
  d->handle_ = gNextHandle.fetch_add(1, std::memory_order_relaxed);
  d->fluxWindow_ = nullptr;
  d->window_ = nil;
  d->metalView_ = nil;
  d->delegate_ = nil;

  NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
  if (config.resizable) {
    styleMask |= NSWindowStyleMaskResizable;
  }

  NSScreen* screen = [NSScreen mainScreen];
  NSRect visible = screen.visibleFrame;
  NSSize size = NSMakeSize(static_cast<CGFloat>(config.size.width), static_cast<CGFloat>(config.size.height));
  CGFloat x = visible.origin.x + (visible.size.width - size.width) * 0.5;
  CGFloat y = visible.origin.y + (visible.size.height - size.height) * 0.5;
  NSRect contentRect = NSMakeRect(x, y, size.width, size.height);

  d->window_ = [[NSWindow alloc] initWithContentRect:contentRect
                                          styleMask:styleMask
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
  [d->window_ setReleasedWhenClosed:NO];

  d->metalView_ = [[FluxMetalView alloc] initWithFrame:[[d->window_ contentView] bounds]];
  d->metalView_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  d->metalView_.fluxPlatform = this;
  [d->window_ setContentView:d->metalView_];

  NSString* title = [NSString stringWithUTF8String:config.title.c_str()];
  if (!title) {
    title = @"";
  }
  [d->window_ setTitle:title];

  d->delegate_ = [[FluxWindowDelegate alloc] init];
  d->delegate_.platform = this;
  [d->window_ setDelegate:d->delegate_];

  if (config.fullscreen) {
    NSWindow* w = d->window_;
    dispatch_async(dispatch_get_main_queue(), ^{
      [w toggleFullScreen:nil];
    });
  }
  // `makeKeyAndOrderFront` is deferred to `show()` so `windowDidBecomeKey` runs after `setFluxWindow`.
}

MacMetalPlatformWindow::~MacMetalPlatformWindow() {
  if (d && d->delegate_) {
    d->delegate_.platform = nullptr;
  }
  if (d && d->window_) {
    [d->window_ setDelegate:nil];
  }
  if (d) {
    d->delegate_ = nil;
    d->metalView_ = nil;
    d->window_ = nil;
  }
  d.reset();
}

void MacMetalPlatformWindow::setFluxWindow(Window* window) {
  d->fluxWindow_ = window;
}

void MacMetalPlatformWindow::show() {
  if (!d->window_ || !d->metalView_) {
    return;
  }
  [d->window_ makeKeyAndOrderFront:nil];
  [d->window_ makeFirstResponder:d->metalView_];
}

void MacMetalPlatformWindow::resize(const Size& newSize) {
  if (!d->window_) {
    return;
  }
  NSSize sz = NSMakeSize(static_cast<CGFloat>(newSize.width), static_cast<CGFloat>(newSize.height));
  [d->window_ setContentSize:sz];
}

void MacMetalPlatformWindow::setFullscreen(bool fullscreen) {
  if (!d->window_) {
    return;
  }
  const bool isFs = ([d->window_ styleMask] & NSWindowStyleMaskFullScreen) != 0;
  if (fullscreen == isFs) {
    return;
  }
  [d->window_ toggleFullScreen:nil];
}

void MacMetalPlatformWindow::setTitle(const std::string& title) {
  if (!d->window_) {
    return;
  }
  NSString* nsTitle = [NSString stringWithUTF8String:title.c_str()];
  if (!nsTitle) {
    nsTitle = @"";
  }
  [d->window_ setTitle:nsTitle];
}

Size MacMetalPlatformWindow::currentSize() const {
  if (!d->window_ || !d->metalView_) {
    return {};
  }
  NSRect bounds = d->metalView_.bounds;
  return Size{static_cast<float>(bounds.size.width), static_cast<float>(bounds.size.height)};
}

bool MacMetalPlatformWindow::isFullscreen() const {
  if (!d->window_) {
    return false;
  }
  return ([d->window_ styleMask] & NSWindowStyleMaskFullScreen) != 0;
}

unsigned int MacMetalPlatformWindow::handle() const {
  return d->handle_;
}

void* MacMetalPlatformWindow::nativeGraphicsSurface() const {
  if (!d->metalView_) {
    return nullptr;
  }
  return (__bridge void*)d->metalView_.layer;
}

std::unique_ptr<Canvas> MacMetalPlatformWindow::createCanvas(Window& owner) {
  (void)owner;
  void* layerPtr = nativeGraphicsSurface();
  if (!layerPtr) {
    return nullptr;
  }
  return createMetalCanvas(&owner, layerPtr, handle(), Application::instance().textSystem());
}

void MacMetalPlatformWindow::processEvents() {
  if (!d->window_) {
    return;
  }
  NSEvent* event = nil;
  while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                   untilDate:[NSDate distantPast]
                                      inMode:NSDefaultRunLoopMode
                                     dequeue:YES])) {
    [NSApp sendEvent:event];
  }
}

void MacMetalPlatformWindow::waitForEvents(int timeoutMs) {
  if (!d->window_) {
    return;
  }
  NSDate* until = (timeoutMs < 0) ? [NSDate distantFuture]
                                : [NSDate dateWithTimeIntervalSinceNow:timeoutMs / 1000.0];
  NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                      untilDate:until
                                         inMode:NSDefaultRunLoopMode
                                        dequeue:YES];
  if (event) {
    [NSApp sendEvent:event];
  }
  while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                     untilDate:[NSDate distantPast]
                                        inMode:NSDefaultRunLoopMode
                                       dequeue:YES])) {
    [NSApp sendEvent:event];
  }
}

void MacMetalPlatformWindow::wakeEventLoop() {
  if (!NSApp) {
    return;
  }
  NSEvent* ev = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                   location:NSZeroPoint
                              modifierFlags:0
                                  timestamp:0
                               windowNumber:0
                                    context:nil
                                    subtype:0
                                      data1:0
                                      data2:0];
  [NSApp postEvent:ev atStart:NO];
}

namespace detail {

std::unique_ptr<PlatformWindow> createPlatformWindow(const WindowConfig& config) {
  return std::make_unique<MacMetalPlatformWindow>(config);
}

} // namespace detail

} // namespace flux
