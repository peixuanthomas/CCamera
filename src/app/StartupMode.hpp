#pragma once

namespace ccamera {

enum class StartupMode {
    DefaultRuntime,
    ConfigRuntime,
};

StartupMode classify_startup_mode(int argc, char** argv);

}  // namespace ccamera
