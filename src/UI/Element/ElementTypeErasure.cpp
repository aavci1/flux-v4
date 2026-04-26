#include <Flux/UI/Element.hpp>

#include <vector>

namespace flux {

namespace detail {

void ElementDeleter::operator()(Element* element) const noexcept {
  delete element;
}

Popover* popoverOverlayStateIf(Element& el) {
  (void)el;
  return nullptr;
}

} // namespace detail

Element::Element(Element const& other) = default;

Element& Element::operator=(Element const& other) = default;

void Element::ensureUniqueImpl() {
  if (impl_ && impl_.use_count() != 1) {
    impl_ = std::shared_ptr<Concept>(impl_->clone());
  }
}

detail::ElementModifiers& Element::writableModifiers() {
  if (!modifiers_) {
    modifiers_ = std::make_shared<detail::ElementModifiers>();
  } else if (modifiers_.use_count() != 1) {
    modifiers_ = std::make_shared<detail::ElementModifiers>(*modifiers_);
  }
  return *modifiers_;
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
