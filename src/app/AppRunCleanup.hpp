#pragma once

#include <functional>

namespace ccamera {

class AppRunCleanup {
public:
    AppRunCleanup(std::function<void()> stop_recognition,
                  std::function<void()> stop_command,
                  std::function<void()> stop_camera);
    ~AppRunCleanup();

    AppRunCleanup(const AppRunCleanup&) = delete;
    AppRunCleanup& operator=(const AppRunCleanup&) = delete;

    void dismiss();

private:
    std::function<void()> stop_recognition_;
    std::function<void()> stop_command_;
    std::function<void()> stop_camera_;
    bool active_ = true;
};

}  // namespace ccamera
