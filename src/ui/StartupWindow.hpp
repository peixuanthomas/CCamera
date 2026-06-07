#pragma once

#include "core/Types.hpp"

#include <optional>

namespace ccamera {

std::optional<AppConfig> show_startup_window(const AppConfig& initial_config);

}  // namespace ccamera
