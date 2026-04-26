#include <Flux/UI/Element.hpp>

namespace flux {

detail::ElementModifiers::ElementModifiers(detail::ElementModifiers const& o)
    : padding(o.padding)
    , fill(o.fill)
    , stroke(o.stroke)
    , shadow(o.shadow)
    , cornerRadius(o.cornerRadius)
    , opacity(o.opacity)
    , translation(o.translation)
    , clip(o.clip)
    , positionX(o.positionX)
    , positionY(o.positionY)
    , sizeWidth(o.sizeWidth)
    , sizeHeight(o.sizeHeight)
    , overlay(o.overlay ? std::make_unique<Element>(*o.overlay) : nullptr)
    , onTap(o.onTap)
    , onPointerEnter(o.onPointerEnter)
    , onPointerExit(o.onPointerExit)
    , onFocus(o.onFocus)
    , onBlur(o.onBlur)
    , onPointerDown(o.onPointerDown)
    , onPointerUp(o.onPointerUp)
    , onPointerMove(o.onPointerMove)
    , onScroll(o.onScroll)
    , onKeyDown(o.onKeyDown)
    , onKeyUp(o.onKeyUp)
    , onTextInput(o.onTextInput)
    , focusable(o.focusable)
    , cursor(o.cursor) {}

detail::ElementModifiers& detail::ElementModifiers::operator=(detail::ElementModifiers const& o) {
  if (this != &o) {
    padding = o.padding;
    fill = o.fill;
    stroke = o.stroke;
    shadow = o.shadow;
    cornerRadius = o.cornerRadius;
    opacity = o.opacity;
    translation = o.translation;
    clip = o.clip;
    positionX = o.positionX;
    positionY = o.positionY;
    sizeWidth = o.sizeWidth;
    sizeHeight = o.sizeHeight;
    overlay = o.overlay ? std::make_unique<Element>(*o.overlay) : nullptr;
    onTap = o.onTap;
    onPointerEnter = o.onPointerEnter;
    onPointerExit = o.onPointerExit;
    onFocus = o.onFocus;
    onBlur = o.onBlur;
    onPointerDown = o.onPointerDown;
    onPointerUp = o.onPointerUp;
    onPointerMove = o.onPointerMove;
    onScroll = o.onScroll;
    onKeyDown = o.onKeyDown;
    onKeyUp = o.onKeyUp;
    onTextInput = o.onTextInput;
    focusable = o.focusable;
    cursor = o.cursor;
  }
  return *this;
}

detail::ElementModifiers::~ElementModifiers() = default;

Element Element::padding(float all) && {
  return std::move(*this).padding(EdgeInsets::uniform(all));
}

Element Element::padding(Reactive::Bindable<EdgeInsets> insets) && {
  writableModifiers().padding = std::move(insets);
  return std::move(*this);
}

Element Element::padding(EdgeInsets insets) && {
  return std::move(*this).padding(Reactive::Bindable<EdgeInsets>(std::move(insets)));
}

Element Element::padding(float top, float right, float bottom, float left) && {
  return std::move(*this).padding(EdgeInsets{.top = top, .right = right, .bottom = bottom, .left = left});
}

Element Element::fill(Reactive::Bindable<FillStyle> style) && {
  writableModifiers().fill = std::move(style);
  return std::move(*this);
}

Element Element::fill(FillStyle style) && {
  return std::move(*this).fill(Reactive::Bindable<FillStyle>(std::move(style)));
}

Element Element::fill(Reactive::Bindable<Color> color) && {
  writableModifiers().fill = Reactive::Bindable<FillStyle>([color = std::move(color)] {
    return FillStyle::solid(color.evaluate());
  });
  return std::move(*this);
}

Element Element::fill(Color color) && {
  return std::move(*this).fill(Reactive::Bindable<Color>(std::move(color)));
}

Element Element::shadow(Reactive::Bindable<ShadowStyle> style) && {
  writableModifiers().shadow = std::move(style);
  return std::move(*this);
}

Element Element::shadow(ShadowStyle style) && {
  return std::move(*this).shadow(Reactive::Bindable<ShadowStyle>(std::move(style)));
}

Element Element::size(Reactive::Bindable<float> width, Reactive::Bindable<float> height) && {
  detail::ElementModifiers& modifiers = writableModifiers();
  modifiers.sizeWidth = std::move(width);
  modifiers.sizeHeight = std::move(height);
  return std::move(*this);
}

Element Element::size(float width, float height) && {
  return std::move(*this).size(Reactive::Bindable<float>(width), Reactive::Bindable<float>(height));
}

Element Element::width(Reactive::Bindable<float> w) && {
  writableModifiers().sizeWidth = std::move(w);
  return std::move(*this);
}

Element Element::width(float w) && {
  return std::move(*this).width(Reactive::Bindable<float>(w));
}

Element Element::height(Reactive::Bindable<float> h) && {
  writableModifiers().sizeHeight = std::move(h);
  return std::move(*this);
}

Element Element::height(float h) && {
  return std::move(*this).height(Reactive::Bindable<float>(h));
}

Element Element::stroke(Reactive::Bindable<StrokeStyle> style) && {
  writableModifiers().stroke = std::move(style);
  return std::move(*this);
}

Element Element::stroke(StrokeStyle style) && {
  return std::move(*this).stroke(Reactive::Bindable<StrokeStyle>(std::move(style)));
}

Element Element::stroke(Reactive::Bindable<Color> color, Reactive::Bindable<float> width) && {
  writableModifiers().stroke = Reactive::Bindable<StrokeStyle>(
      [color = std::move(color), width = std::move(width)] {
        return StrokeStyle::solid(color.evaluate(), width.evaluate());
      });
  return std::move(*this);
}

Element Element::stroke(Color color, float width) && {
  return std::move(*this).stroke(Reactive::Bindable<Color>(std::move(color)), Reactive::Bindable<float>(width));
}

Element Element::cornerRadius(Reactive::Bindable<CornerRadius> radius) && {
  writableModifiers().cornerRadius = std::move(radius);
  return std::move(*this);
}

Element Element::cornerRadius(CornerRadius radius) && {
  return std::move(*this).cornerRadius(Reactive::Bindable<CornerRadius>(radius));
}

Element Element::cornerRadius(Reactive::Bindable<float> radius) && {
  writableModifiers().cornerRadius = Reactive::Bindable<CornerRadius>([radius = std::move(radius)] {
    return CornerRadius(radius.evaluate());
  });
  return std::move(*this);
}

Element Element::cornerRadius(float radius) && {
  return std::move(*this).cornerRadius(Reactive::Bindable<float>(radius));
}

Element Element::opacity(Reactive::Bindable<float> opacity) && {
  writableModifiers().opacity = std::move(opacity);
  return std::move(*this);
}

Element Element::opacity(float opacity) && {
  return std::move(*this).opacity(Reactive::Bindable<float>(opacity));
}

Element Element::position(Reactive::Bindable<Vec2> p) && {
  writableModifiers().positionX = Reactive::Bindable<float>([p] { return p.evaluate().x; });
  writableModifiers().positionY = Reactive::Bindable<float>([p] { return p.evaluate().y; });
  return std::move(*this);
}

Element Element::position(Vec2 p) && {
  return std::move(*this).position(Reactive::Bindable<Vec2>(p));
}

Element Element::position(Reactive::Bindable<float> x, Reactive::Bindable<float> y) && {
  detail::ElementModifiers& modifiers = writableModifiers();
  modifiers.positionX = std::move(x);
  modifiers.positionY = std::move(y);
  return std::move(*this);
}

Element Element::position(float x, float y) && {
  return std::move(*this).position(Reactive::Bindable<float>(x), Reactive::Bindable<float>(y));
}

Element Element::translate(Reactive::Bindable<Vec2> delta) && {
  writableModifiers().translation = std::move(delta);
  return std::move(*this);
}

Element Element::translate(Vec2 delta) && {
  return std::move(*this).translate(Reactive::Bindable<Vec2>(delta));
}

Element Element::translate(Reactive::Bindable<float> dx, Reactive::Bindable<float> dy) && {
  writableModifiers().translation = Reactive::Bindable<Vec2>(
      [dx = std::move(dx), dy = std::move(dy)] {
        return Vec2{dx.evaluate(), dy.evaluate()};
      });
  return std::move(*this);
}

Element Element::translate(float dx, float dy) && {
  return std::move(*this).translate(Reactive::Bindable<float>(dx), Reactive::Bindable<float>(dy));
}

Element Element::clipContent(bool clip) && {
  writableModifiers().clip = clip;
  return std::move(*this);
}

Element Element::overlay(Element over) && {
  writableModifiers().overlay = std::make_unique<Element>(std::move(over));
  return std::move(*this);
}

Element Element::onTap(std::function<void()> handler) && {
  writableModifiers().onTap = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerEnter(std::function<void()> handler) && {
  writableModifiers().onPointerEnter = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerExit(std::function<void()> handler) && {
  writableModifiers().onPointerExit = std::move(handler);
  return std::move(*this);
}

Element Element::onFocus(std::function<void()> handler) && {
  writableModifiers().onFocus = std::move(handler);
  return std::move(*this);
}

Element Element::onBlur(std::function<void()> handler) && {
  writableModifiers().onBlur = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerDown(std::function<void(Point)> handler) && {
  writableModifiers().onPointerDown = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerUp(std::function<void(Point)> handler) && {
  writableModifiers().onPointerUp = std::move(handler);
  return std::move(*this);
}

Element Element::onPointerMove(std::function<void(Point)> handler) && {
  writableModifiers().onPointerMove = std::move(handler);
  return std::move(*this);
}

Element Element::onScroll(std::function<void(Vec2)> handler) && {
  writableModifiers().onScroll = std::move(handler);
  return std::move(*this);
}

Element Element::onKeyDown(std::function<void(KeyCode, Modifiers)> handler) && {
  writableModifiers().onKeyDown = std::move(handler);
  return std::move(*this);
}

Element Element::onKeyUp(std::function<void(KeyCode, Modifiers)> handler) && {
  writableModifiers().onKeyUp = std::move(handler);
  return std::move(*this);
}

Element Element::onTextInput(std::function<void(std::string const&)> handler) && {
  writableModifiers().onTextInput = std::move(handler);
  return std::move(*this);
}

Element Element::focusable(bool enabled) && {
  writableModifiers().focusable = enabled;
  return std::move(*this);
}

Element Element::cursor(Cursor c) && {
  writableModifiers().cursor = c;
  return std::move(*this);
}

} // namespace flux
