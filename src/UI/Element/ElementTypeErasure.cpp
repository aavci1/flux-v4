#include <Flux/UI/Element.hpp>

#include <Flux/UI/Views/Popover.hpp>

namespace flux {

namespace detail {

std::uint64_t nextElementMeasureId() {
  static std::uint64_t n = 1;
  return n++;
}

Popover* popoverOverlayStateIf(Element& el) {
  auto* m = dynamic_cast<Element::Model<Popover>*>(el.impl_.get());
  return m ? &m->value : nullptr;
}

} // namespace detail

Element::Element(Element const& other)
    : impl_(other.impl_ ? other.impl_->clone() : nullptr)
    , flexGrowOverride_(other.flexGrowOverride_)
    , flexShrinkOverride_(other.flexShrinkOverride_)
    , flexBasisOverride_(other.flexBasisOverride_)
    , minMainSizeOverride_(other.minMainSizeOverride_)
    , envLayer_(other.envLayer_)
    , modifiers_(other.modifiers_)
    , key_(other.key_)
    , measureId_(detail::nextElementMeasureId()) {}

Element& Element::operator=(Element const& other) {
  if (this != &other) {
    impl_ = other.impl_ ? other.impl_->clone() : nullptr;
    flexGrowOverride_ = other.flexGrowOverride_;
    flexShrinkOverride_ = other.flexShrinkOverride_;
    flexBasisOverride_ = other.flexBasisOverride_;
    minMainSizeOverride_ = other.minMainSizeOverride_;
    envLayer_ = other.envLayer_;
    modifiers_ = other.modifiers_;
    key_ = other.key_;
    measureId_ = detail::nextElementMeasureId();
  }
  return *this;
}

float Element::flexGrow() const {
  return flexGrowOverride_.value_or(impl_->flexGrow());
}

float Element::flexShrink() const {
  return flexShrinkOverride_.value_or(impl_->flexShrink());
}

std::optional<float> Element::flexBasis() const {
  if (flexBasisOverride_.has_value()) {
    return flexBasisOverride_;
  }
  return impl_->flexBasis();
}

float Element::minMainSize() const {
  return minMainSizeOverride_.value_or(impl_->minMainSize());
}

Element Element::flex(float grow) && {
  flexGrowOverride_ = grow;
  flexShrinkOverride_ = 1.f;
  flexBasisOverride_.reset();
  minMainSizeOverride_.reset();
  return std::move(*this);
}

Element Element::flex(float grow, float shrink) && {
  flexGrowOverride_ = grow;
  flexShrinkOverride_ = shrink;
  flexBasisOverride_.reset();
  minMainSizeOverride_.reset();
  return std::move(*this);
}

Element Element::flex(float grow, float shrink, float basis) && {
  flexGrowOverride_ = grow;
  flexShrinkOverride_ = shrink;
  flexBasisOverride_ = std::max(0.f, basis);
  minMainSizeOverride_.reset();
  return std::move(*this);
}

Element Element::key(std::string key) && {
  key_ = std::move(key);
  return std::move(*this);
}

} // namespace flux
