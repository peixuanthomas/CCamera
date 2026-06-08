#include "app/StartupMode.hpp"

#include <cassert>

int main() {
    const char* no_args[] = {"ccamera.exe"};
    const auto default_start = ccamera::classify_startup_mode(1, const_cast<char**>(no_args));
    assert(default_start == ccamera::StartupMode::DefaultRuntime);

    const char* config_args[] = {"ccamera.exe", "--config", "config/app.example.json"};
    const auto config_start = ccamera::classify_startup_mode(3, const_cast<char**>(config_args));
    assert(config_start == ccamera::StartupMode::ConfigRuntime);

    const char* positional_args[] = {"ccamera.exe", "config/app.example.json"};
    const auto positional_start = ccamera::classify_startup_mode(2, const_cast<char**>(positional_args));
    assert(positional_start == ccamera::StartupMode::ConfigRuntime);

    return 0;
}
