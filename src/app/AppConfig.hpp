#pragma once

#include "core/Types.hpp"

#include <string>

namespace ccamera {

AppConfig load_app_config(const std::string& path);
AppConfig load_app_config_from_args(int argc, char** argv);
void save_app_config(const std::string& path, const AppConfig& config);
void validate_app_config(const AppConfig& config);

}  // namespace ccamera
