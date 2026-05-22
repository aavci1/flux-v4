#pragma once

#include "Shell/UI/LambdaShellTypes.hpp"

#include <Flux/UI/Element.hpp>

namespace lambda_shell {

struct LambdaTopBar {
  TopBarProps props;

  flux::Element body() const;
};

} // namespace lambda_shell
