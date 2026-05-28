#pragma once

#include "Shell/UI/LambdaShellTypes.hpp"

#include <Lambda/UI/Element.hpp>

namespace lambda_shell {

struct LambdaTopBar {
  TopBarProps props;

  lambda::Element body() const;
};

} // namespace lambda_shell
