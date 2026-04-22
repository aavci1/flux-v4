#include <Flux/UI/Element.hpp>

#include <Flux/UI/Views/Popover.hpp>

#include <vector>

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

namespace {

struct EnvironmentPushScope {
  std::size_t count = 0;

  ~EnvironmentPushScope() {
    EnvironmentStack& environment = EnvironmentStack::current();
    while (count-- > 0) {
      environment.pop();
    }
  }
};

} // namespace

Element::Element(Element const& other)
    : impl_(other.impl_ ? other.impl_->clone() : nullptr)
    , flexGrowOverride_(other.flexGrowOverride_)
    , flexShrinkOverride_(other.flexShrinkOverride_)
    , flexBasisOverride_(other.flexBasisOverride_)
    , minMainSizeOverride_(other.minMainSizeOverride_)
    , envLayer_(other.envLayer_)
    , modifiers_(other.modifiers_)
    , key_(other.key_)
    , measureId_(other.measureId_) {}

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
    measureId_ = other.measureId_;
  }
  return *this;
}

Element Element::strippedEnvelopeCopy() const {
  Element copy = *this;
  copy.envLayer_.reset();
  copy.modifiers_.reset();
  copy.key_.reset();
  return copy;
}

detail::ResolvedElement Element::resolve(ComponentKey const& key,
                                         LayoutConstraints const& constraints) const {
  detail::ResolvedElement resolved{};
  EnvironmentPushScope pushedEnvironments{};
  std::vector<std::unique_ptr<Element>> ownedBodies{};
  Element const* current = this;
  ComponentKey currentKey = key;
  bool expandedAnyBody = false;
  bool descendantsStable = true;

  while (current) {
    if (EnvironmentLayer const* envLayer = current->environmentLayer()) {
      resolved.environmentLayers.push_back(*envLayer);
      EnvironmentStack::current().push(*envLayer);
      ++pushedEnvironments.count;
    }
    if (detail::ElementModifiers const* modifiers = current->modifiers()) {
      resolved.modifierLayers.push_back(*modifiers);
    }
    if (!current->expandsBody()) {
      resolved.sceneElement = std::make_unique<Element>(current->strippedEnvelopeCopy());
      resolved.descendantsStable = expandedAnyBody && descendantsStable;
      return resolved;
    }

    if (expandedAnyBody) {
      resolved.bodyComponentKeys.push_back(currentKey);
    }
    expandedAnyBody = true;
    detail::CompositeBodyResolution bodyResolution = current->resolveCompositeBody(currentKey, constraints);
    descendantsStable = descendantsStable && bodyResolution.descendantsStable;
    if (!bodyResolution.body) {
      resolved.sceneElement = std::make_unique<Element>(current->strippedEnvelopeCopy());
      resolved.descendantsStable = false;
      return resolved;
    }

    if (bodyResolution.ownedBody) {
      ownedBodies.push_back(std::move(bodyResolution.ownedBody));
      current = ownedBodies.back().get();
    } else {
      current = bodyResolution.body;
    }
    currentKey.push_back(detail::compositeBodyLocalId());
  }

  resolved.sceneElement = std::make_unique<Element>(strippedEnvelopeCopy());
  resolved.descendantsStable = false;
  return resolved;
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
