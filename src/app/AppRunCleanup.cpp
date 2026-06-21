#include "app/AppRunCleanup.hpp"

namespace ccamera {

AppRunCleanup::AppRunCleanup(std::function<void()> stop_recognition,
                             std::function<void()> stop_command,
                             std::function<void()> stop_web,
                             std::function<void()> stop_camera)
    : stop_recognition_(std::move(stop_recognition)),
      stop_command_(std::move(stop_command)),
      stop_web_(std::move(stop_web)),
      stop_camera_(std::move(stop_camera)) {}

AppRunCleanup::~AppRunCleanup() {
    if (!active_) {
        return;
    }
    if (stop_recognition_) {
        stop_recognition_();
    }
    if (stop_command_) {
        stop_command_();
    }
    if (stop_web_) {
        stop_web_();
    }
    if (stop_camera_) {
        stop_camera_();
    }
}

void AppRunCleanup::dismiss() {
    active_ = false;
}

}  // namespace ccamera
