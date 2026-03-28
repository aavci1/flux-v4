#pragma once

#include <Flux/UI/Component.hpp>
#include <Flux/UI/Leaves.hpp>
#include <Flux/UI/LayoutEngine.hpp>

#include <memory>
#include <type_traits>
#include <utility>

namespace flux {

template<typename>
inline constexpr bool alwaysFalse = false;


class BuildContext;
class TextSystem;

class Element {
public:
  template<typename C>
  Element(C component);

  /// Copying clones the type-erased implementation so `std::vector<Element>` and
  /// brace-initialized child lists (which copy via `std::initializer_list`) work.
  Element(Element const& other);
  Element& operator=(Element const& other);
  Element(Element&&) noexcept = default;
  Element& operator=(Element&&) noexcept = default;

  void build(BuildContext& ctx) const;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const;

  /// Used by layout containers to distribute flexible space among `Spacer` children.
  bool isSpacer() const;

private:
  friend class LayoutEngine;

  struct Concept {
    virtual ~Concept() = default;
    virtual std::unique_ptr<Concept> clone() const = 0;
    virtual void build(BuildContext& ctx) const = 0;
    virtual Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const = 0;
    virtual bool isSpacer() const { return false; }
  };

  template<typename C>
  struct Model;

  std::unique_ptr<Concept> impl_;
};

template<typename C>
struct Element::Model : Concept {
  C value;
  explicit Model(C c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<C>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<typename C>
void Element::Model<C>::build(BuildContext& ctx) const {
  if constexpr (CompositeComponent<C>) {
    Element child{value.body()};
    child.build(ctx);
  } else {
    static_assert(alwaysFalse<C>, "Missing Element::Model specialization for this component type");
  }
}

template<typename C>
Size Element::Model<C>::measure(LayoutConstraints const& constraints, TextSystem& textSystem) const {
  if constexpr (CompositeComponent<C>) {
    Element child{value.body()};
    return child.measure(constraints, textSystem);
  } else {
    static_assert(alwaysFalse<C>, "Missing Element::Model specialization for this component type");
    return {};
  }
}

} // namespace flux

#include <Flux/UI/Layout.hpp>

namespace flux {

template<>
struct Element::Model<Rectangle> final : Concept {
  Rectangle value;
  explicit Model(Rectangle c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Rectangle>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<LaidOutText> final : Concept {
  LaidOutText value;
  explicit Model(LaidOutText c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<LaidOutText>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<Text> final : Concept {
  Text value;
  explicit Model(Text c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Text>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<views::Image> final : Concept {
  views::Image value;
  explicit Model(views::Image c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<views::Image>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<PathShape> final : Concept {
  PathShape value;
  explicit Model(PathShape c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<PathShape>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<Line> final : Concept {
  Line value;
  explicit Model(Line c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Line>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<VStack> final : Concept {
  VStack value;
  explicit Model(VStack c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<VStack>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<HStack> final : Concept {
  HStack value;
  explicit Model(HStack c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<HStack>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<ZStack> final : Concept {
  ZStack value;
  explicit Model(ZStack c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<ZStack>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
};

template<>
struct Element::Model<Spacer> final : Concept {
  Spacer value;
  explicit Model(Spacer c) : value(std::move(c)) {}
  std::unique_ptr<Concept> clone() const override {
    return std::make_unique<Model<Spacer>>(value);
  }
  void build(BuildContext& ctx) const override;
  Size measure(LayoutConstraints const& constraints, TextSystem& textSystem) const override;
  bool isSpacer() const override { return true; }
};

template<typename C>
Element::Element(C component) : impl_(std::make_unique<Model<C>>(std::move(component))) {}

inline Element::Element(Element const& other)
    : impl_(other.impl_ ? other.impl_->clone() : nullptr) {}

inline Element& Element::operator=(Element const& other) {
  if (this != &other) {
    impl_ = other.impl_ ? other.impl_->clone() : nullptr;
  }
  return *this;
}

} // namespace flux
