#import <Cocoa/Cocoa.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <Flux/Core/Application.hpp>
#include <Flux/Core/Cursor.hpp>
#include <Flux/Core/EventQueue.hpp>
#include <Flux/Core/Events.hpp>
#include <Flux/Core/Application.hpp>
#include <Flux/Core/Window.hpp>
#include <Flux/Graphics/TextSystem.hpp>
#include <Flux/Reactive/Profile.hpp>

#include "Core/PlatformWindow.hpp"
#include "Core/PlatformWindowCreate.hpp"
#include "Graphics/Metal/MetalCanvas.hpp"
#include "UI/DebugFlags.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <dispatch/dispatch.h>
#include <memory>
#include <string>

namespace flux {
class MacMetalPlatformWindow;
class Window;
Window* fluxWindowForPlatform(MacMetalPlatformWindow* platform);
CVReturn fluxHandleDisplayLinkTick(MacMetalPlatformWindow* platform);
} // namespace flux

/// Private AppKit class methods; stable in practice for diagonal window-resize cursors.
@interface NSCursor (FluxPrivateResizeCursors)
+ (NSCursor*)_windowResizeNorthEastSouthWestCursor;
+ (NSCursor*)_windowResizeNorthWestSouthEastCursor;
@end

@interface FluxMetalView : NSView <NSTextInputClient>
@property(nonatomic, assign) flux::MacMetalPlatformWindow* fluxPlatform;
- (CAMetalLayer*)fluxMetalLayer;
- (void)updateDrawableSize;
- (BOOL)fluxWantsTextInput;
- (void)fluxHandleDisplayLink:(CADisplayLink*)displayLink API_AVAILABLE(macos(14.0));
@end

namespace flux {
namespace detail {
void postInputFromView(FluxMetalView* view, InputEvent::Kind kind, NSEvent* e, std::string text = {});
void postTextInput(FluxMetalView* view, std::string text);
} // namespace detail
} // namespace flux

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

  // `presentsWithTransaction` is toggled only around resize-driven flush (see windowDidResize). Leaving it
  // always YES can defer the first composite until a later CA transaction and cause an intermittent blank window.
  metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
  metalLayer.needsDisplayOnBoundsChange = YES;

  return metalLayer;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;
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

- (void)cursorUpdate:(NSEvent*)event {
  // Suppress AppKit's automatic reset to the arrow cursor.
  // Runtime::updateCursorForPoint handles cursor selection after every move.
  (void)event;
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

- (void)keyDown:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::KeyDown, event);
  if ([self fluxWantsTextInput]) {
    [self interpretKeyEvents:@[event]];
  }
}

- (void)keyUp:(NSEvent*)event {
  flux::detail::postInputFromView(self, flux::InputEvent::Kind::KeyUp, event);
}

- (void)doCommandBySelector:(SEL)selector {
  (void)selector;
}

- (BOOL)hasMarkedText {
  return NO;
}

- (NSRange)markedRange {
  return NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
  return NSMakeRange(NSNotFound, 0);
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  (void)string;
  (void)selectedRange;
  (void)replacementRange;
}

- (void)unmarkText {
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText {
  return @[];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actualRange {
  (void)range;
  (void)actualRange;
  return nil;
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  (void)replacementRange;
  NSString* s = nil;
  if ([string isKindOfClass:[NSAttributedString class]]) {
    s = [(NSAttributedString*)string string];
  } else if ([string isKindOfClass:[NSString class]]) {
    s = (NSString*)string;
  }
  std::string utf8 = s ? [s UTF8String] : "";
  flux::detail::postTextInput(self, std::move(utf8));
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
  (void)point;
  return NSNotFound;
}

- (BOOL)fluxWantsTextInput {
  flux::MacMetalPlatformWindow* platform = self.fluxPlatform;
  flux::Window* window = flux::fluxWindowForPlatform(platform);
  return window && window->wantsTextInput();
}

- (NSTextInputContext*)inputContext {
  if (![self fluxWantsTextInput]) {
    return nil;
  }
  return [super inputContext];
}

- (void)fluxHandleDisplayLink:(CADisplayLink*)displayLink {
  (void)displayLink;
  flux::MacMetalPlatformWindow* platform = self.fluxPlatform;
  if (!platform) {
    return;
  }
  (void)flux::fluxHandleDisplayLinkTick(platform);
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
  (void)range;
  (void)actualRange;
  NSWindow* w = self.window;
  return w ? [w convertRectToScreen:w.frame] : NSZeroRect;
}

@end

@interface FluxWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) flux::MacMetalPlatformWindow* platform;
@end

namespace flux {

namespace {

std::atomic<unsigned int> gNextHandle{1};

std::int64_t nowSteadyClockNanos() {
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

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
  void requestAnimationFrame() override;
  void acknowledgeAnimationFrameTick() override;
  void completeAnimationFrame(bool needsAnotherFrame) override;

  void setCursor(Cursor kind) override;

  Window* fluxWindow() const;
  CVReturn onDisplayLinkTick();

  /// Enables CAMetalLayer transaction presentation only for resize flushes (paired with MetalCanvas sync present).
  void setMetalLayerPresentsWithTransaction(bool enable);

private:
  void setModernDisplayLinkPaused(bool paused);

  struct Impl;
  std::unique_ptr<Impl> d;
};

CVReturn displayLinkOutputCallback(CVDisplayLinkRef /*displayLink*/, CVTimeStamp const* /*now*/,
                                   CVTimeStamp const* /*outputTime*/, CVOptionFlags /*flagsIn*/,
                                   CVOptionFlags* /*flagsOut*/, void* userInfo) {
  auto* platform = static_cast<MacMetalPlatformWindow*>(userInfo);
  if (!platform) {
    return kCVReturnSuccess;
  }
  return platform->onDisplayLinkTick();
}

struct MacMetalPlatformWindow::Impl {
  NSWindow* window_{nil};
  FluxMetalView* metalView_{nil};
  FluxWindowDelegate* delegate_{nil};
  Window* fluxWindow_{nullptr};
  unsigned int handle_{0};
  CADisplayLink* displayLink_ = nil;
  CVDisplayLinkRef legacyDisplayLink_{nullptr};
  std::atomic<bool> frameRequested_{false};
  std::atomic<bool> frameEventQueued_{false};
  std::atomic<bool> legacyDisplayLinkRunning_{false};
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

bool fluxDebugInputMacPost() {
  return debug::inputEnabled();
}

void postInputFromView(FluxMetalView* view, InputEvent::Kind kind, NSEvent* e, std::string text) {
  MacMetalPlatformWindow* p = view.fluxPlatform;
  if (!p || !p->fluxWindow()) {
    if (fluxDebugInputMacPost()) {
      std::fprintf(stderr, "[flux:input:mac] postInputFromView: no platform/window (dropped)\n");
    }
    return;
  }
  InputEvent ie;
  ie.kind = kind;
  ie.handle = p->fluxWindow()->handle();
  if (kind == InputEvent::Kind::Scroll) {
    NSPoint pt = [view convertPoint:[e locationInWindow] fromView:nil];
    ie.position = Vec2{static_cast<float>(pt.x), static_cast<float>(pt.y)};
    ie.scrollDelta =
        Vec2{static_cast<float>(e.scrollingDeltaX), static_cast<float>(e.scrollingDeltaY)};
    ie.preciseScrollDelta = static_cast<bool>(e.hasPreciseScrollingDeltas);
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
  {
    std::uint8_t pb = static_cast<std::uint8_t>([NSEvent pressedMouseButtons] & 0xFF);
    // Session-state can reflect a physical release before AppKit's bitmask updates when mouseUp
    // was not delivered to this window (e.g. release outside the window during a drag).
    if (!CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState, kCGMouseButtonLeft)) {
      pb &= static_cast<std::uint8_t>(~1u);
    }
    ie.pressedButtons = pb;
  }
  ie.text = std::move(text);
  if (fluxDebugInputMacPost()) {
    if (kind == InputEvent::Kind::Scroll) {
      std::fprintf(stderr,
                   "[flux:input:mac] post Scroll handle=%u pos=(%.1f,%.1f) delta=(%.2f,%.2f)\n",
                   static_cast<unsigned>(ie.handle), static_cast<double>(ie.position.x),
                   static_cast<double>(ie.position.y), static_cast<double>(ie.scrollDelta.x),
                   static_cast<double>(ie.scrollDelta.y));
    } else if (kind == InputEvent::Kind::PointerMove) {
      static int moveN;
      if (++moveN % 20 == 1) {
        std::fprintf(stderr, "[flux:input:mac] post PointerMove handle=%u pos=(%.1f,%.1f) (sampled)\n",
                     static_cast<unsigned>(ie.handle), static_cast<double>(ie.position.x),
                     static_cast<double>(ie.position.y));
      }
    } else {
      char const* kn = "?";
      switch (kind) {
      case InputEvent::Kind::PointerDown:
        kn = "PointerDown";
        break;
      case InputEvent::Kind::PointerUp:
        kn = "PointerUp";
        break;
      default:
        break;
      }
      std::fprintf(stderr, "[flux:input:mac] post %s handle=%u pos=(%.1f,%.1f)\n", kn,
                   static_cast<unsigned>(ie.handle), static_cast<double>(ie.position.x),
                   static_cast<double>(ie.position.y));
    }
  }
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
  // Live resize runs in NSEventTrackingRunLoopMode; our main loop waits in NSDefaultRunLoopMode, so it does not
  // run the redraw pass until tracking ends. Dispatch + flush presents immediately during the drag.
  flux::Application::instance().eventQueue().dispatch();
  // `flushRedraw` only presents when `requestRedraw` has been set. Declarative windows get this from
  // `Runtime`'s resize subscription; imperative apps must not rely on that — always request here.
  flux::Application::instance().requestRedraw();
  platform->setMetalLayerPresentsWithTransaction(true);
  flux::setSyncPresentForCanvas(&fw->canvas(), true);
  flux::Application::instance().flushRedraw();
  platform->setMetalLayerPresentsWithTransaction(false);
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

@end

namespace flux {

Window* fluxWindowForPlatform(MacMetalPlatformWindow* platform) {
  return platform ? platform->fluxWindow() : nullptr;
}

CVReturn fluxHandleDisplayLinkTick(MacMetalPlatformWindow* platform) {
  if (!platform) {
    return kCVReturnSuccess;
  }
  return platform->onDisplayLinkTick();
}

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
  // Avoid scaling a stale snapshot during live resize; custom Metal content must update each frame.
  d->window_.preservesContentDuringLiveResize = NO;

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
  if (@available(macOS 14.0, *)) {
    d->displayLink_ = [d->metalView_ displayLinkWithTarget:d->metalView_
                                                  selector:@selector(fluxHandleDisplayLink:)];
    if (d->displayLink_) {
      [d->displayLink_ addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
      d->displayLink_.paused = YES;
    }
  }
  if (!d->displayLink_) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CVDisplayLinkCreateWithActiveCGDisplays(&d->legacyDisplayLink_);
    if (d->legacyDisplayLink_) {
      CVDisplayLinkSetOutputCallback(d->legacyDisplayLink_, displayLinkOutputCallback, this);
    }
#pragma clang diagnostic pop
  }
  // `makeKeyAndOrderFront` is deferred to `show()` so `windowDidBecomeKey` runs after `setFluxWindow`.
}

MacMetalPlatformWindow::~MacMetalPlatformWindow() {
  if (d && d->displayLink_) {
    [d->displayLink_ invalidate];
    d->displayLink_ = nil;
  }
  if (d && d->legacyDisplayLink_) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CVDisplayLinkStop(d->legacyDisplayLink_);
    CVDisplayLinkRelease(d->legacyDisplayLink_);
#pragma clang diagnostic pop
    d->legacyDisplayLink_ = nullptr;
  }
  if (d && d->delegate_) {
    d->delegate_.platform = nullptr;
  }
  if (d && d->window_) {
    [d->window_ setDelegate:nil];
  }
  if (d) {
    if (d->metalView_) {
      d->metalView_.fluxPlatform = nullptr;
    }
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

void MacMetalPlatformWindow::setMetalLayerPresentsWithTransaction(bool enable) {
  if (!d->metalView_) {
    return;
  }
  CAMetalLayer* ml = [d->metalView_ fluxMetalLayer];
  if (ml) {
    ml.presentsWithTransaction = enable ? YES : NO;
  }
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
  auto postWakeEvent = ^{
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
  };
  if ([NSThread isMainThread]) {
    postWakeEvent();
  } else {
    CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopDefaultMode, postWakeEvent);
    CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopCommonModes, postWakeEvent);
  }
  CFRunLoopWakeUp(CFRunLoopGetMain());
}

void MacMetalPlatformWindow::requestAnimationFrame() {
  d->frameRequested_.store(true, std::memory_order_release);
  if (d->displayLink_) {
    setModernDisplayLinkPaused(false);
    return;
  }
  if (!d->legacyDisplayLink_) {
    return;
  }
  if (!d->legacyDisplayLinkRunning_.exchange(true, std::memory_order_acq_rel)) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CVDisplayLinkStart(d->legacyDisplayLink_);
#pragma clang diagnostic pop
  }
}

void MacMetalPlatformWindow::acknowledgeAnimationFrameTick() {
  d->frameEventQueued_.store(false, std::memory_order_release);
}

void MacMetalPlatformWindow::completeAnimationFrame(bool needsAnotherFrame) {
  d->frameRequested_.store(needsAnotherFrame, std::memory_order_release);
  if (d->displayLink_) {
    if (!needsAnotherFrame) {
      setModernDisplayLinkPaused(true);
    }
    return;
  }
  if (needsAnotherFrame || !d->legacyDisplayLink_) {
    return;
  }
  if (d->legacyDisplayLinkRunning_.exchange(false, std::memory_order_acq_rel)) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CVDisplayLinkStop(d->legacyDisplayLink_);
#pragma clang diagnostic pop
  }
}

CVReturn MacMetalPlatformWindow::onDisplayLinkTick() {
  Reactive::detail::profile::frameBoundary();
  if (!d->frameRequested_.load(std::memory_order_acquire)) {
    return kCVReturnSuccess;
  }
  if (!Application::hasInstance()) {
    return kCVReturnSuccess;
  }
  bool expected = false;
  if (!d->frameEventQueued_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
    return kCVReturnSuccess;
  }
  FrameEvent event{};
  event.deadlineNanos = nowSteadyClockNanos();
  event.windowHandle = handle();
  if ([NSThread isMainThread]) {
    Application& app = Application::instance();
    app.eventQueue().post(event);
    app.eventQueue().dispatch();
    app.flushRedraw();
    return kCVReturnSuccess;
  }
  Application::instance().eventQueue().post(event);
  wakeEventLoop();
  return kCVReturnSuccess;
}

void MacMetalPlatformWindow::setModernDisplayLinkPaused(bool paused) {
  CADisplayLink* link = d ? d->displayLink_ : nil;
  if (!link) {
    return;
  }
  void (^updatePausedState)(void) = ^{
    link.paused = paused ? YES : NO;
  };
  if ([NSThread isMainThread]) {
    updatePausedState();
    return;
  }
  CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopCommonModes, updatePausedState);
  CFRunLoopWakeUp(CFRunLoopGetMain());
}

void MacMetalPlatformWindow::setCursor(Cursor kind) {
  NSCursor* c = nil;
  switch (kind) {
  case Cursor::Inherit:
    c = [NSCursor arrowCursor];
    break;
  case Cursor::Arrow:
    c = [NSCursor arrowCursor];
    break;
  case Cursor::IBeam:
    c = [NSCursor IBeamCursor];
    break;
  case Cursor::Hand:
    c = [NSCursor pointingHandCursor];
    break;
  case Cursor::ResizeEW:
    c = [NSCursor resizeLeftRightCursor];
    break;
  case Cursor::ResizeNS:
    c = [NSCursor resizeUpDownCursor];
    break;
  case Cursor::ResizeNESW:
    // Private API; public alternatives are resizeLeftRight/resizeUpDown only.
    c = [NSCursor _windowResizeNorthEastSouthWestCursor];
    break;
  case Cursor::ResizeNWSE:
    c = [NSCursor _windowResizeNorthWestSouthEastCursor];
    break;
  case Cursor::ResizeAll:
    c = [NSCursor openHandCursor];
    break;
  case Cursor::Crosshair:
    c = [NSCursor crosshairCursor];
    break;
  case Cursor::NotAllowed:
    c = [NSCursor operationNotAllowedCursor];
    break;
  }
  if (c) {
    [c set];
  }
}

namespace detail {

std::unique_ptr<PlatformWindow> createPlatformWindow(const WindowConfig& config) {
  return std::make_unique<MacMetalPlatformWindow>(config);
}

} // namespace detail

} // namespace flux
