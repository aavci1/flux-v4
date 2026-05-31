#pragma once

#include "Shell/UI/LambdaShellTypes.hpp"

#include <filesystem>

namespace lambda_shell {

[[nodiscard]] SystemStatus readShellSystemStatus(std::filesystem::path sysRoot = "/sys");

} // namespace lambda_shell
