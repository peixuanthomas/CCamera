#include "app/StartupMode.hpp"

namespace ccamera {

StartupMode classify_startup_mode(int argc, char**) {
    if (argc <= 1) {
        return StartupMode::DefaultRuntime;
    }
    return StartupMode::ConfigRuntime;
}

}  // namespace ccamera
