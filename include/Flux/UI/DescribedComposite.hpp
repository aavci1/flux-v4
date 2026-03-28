#pragma once

namespace flux {

/// Wraps a copyable description `Desc` for a heap-managed composite state type `C` (registry).
/// Use when `C` cannot be move-constructed into `Element` (e.g. non-movable reactive fields).
template<typename C, typename Desc>
struct DescribedComposite {
  Desc desc;
};

} // namespace flux
