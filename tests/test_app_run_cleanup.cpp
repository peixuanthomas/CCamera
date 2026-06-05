#include "app/AppRunCleanup.hpp"

#include <cassert>
#include <stdexcept>

int main() {
    bool recognition_stopped = false;
    bool command_stopped = false;
    bool camera_stopped = false;

    try {
        ccamera::AppRunCleanup cleanup(
            [&] { recognition_stopped = true; },
            [&] { command_stopped = true; },
            [&] { camera_stopped = true; });
        throw std::runtime_error("preview loop failed");
    } catch (const std::runtime_error&) {
    }

    assert(recognition_stopped);
    assert(command_stopped);
    assert(camera_stopped);

    recognition_stopped = false;
    command_stopped = false;
    camera_stopped = false;

    {
        ccamera::AppRunCleanup cleanup(
            [&] { recognition_stopped = true; },
            [&] { command_stopped = true; },
            [&] { camera_stopped = true; });
        cleanup.dismiss();
    }

    assert(!recognition_stopped);
    assert(!command_stopped);
    assert(!camera_stopped);

    return 0;
}
