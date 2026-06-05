#include "camera/CameraCapture.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

namespace {

class FailingOpenSource final : public ccamera::CameraSource {
public:
    bool open(int) override { return false; }
    bool is_opened() const override { return false; }
    void set(int, double) override {}
    bool read(cv::Mat&) override { return false; }
};

class FailingReadSource final : public ccamera::CameraSource {
public:
    bool open(int) override { return true; }
    bool is_opened() const override { return true; }
    void set(int, double) override {}
    bool read(cv::Mat&) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return false;
    }
};

bool wait_for_error(ccamera::CameraCapture& camera, const std::string& expected) {
    for (int i = 0; i < 100; ++i) {
        const auto status = camera.status();
        if (status.last_error.find(expected) != std::string::npos) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

}  // namespace

int main() {
    {
        ccamera::FrameBuffer buffer;
        ccamera::AppConfig config;
        config.camera_index = 99;
        ccamera::CameraCapture camera(config, buffer, [] { return std::make_unique<FailingOpenSource>(); });

        camera.start();
        assert(wait_for_error(camera, "open camera"));
        const auto status = camera.status();
        assert(!status.opened);
        assert(!status.last_error.empty());
    }

    {
        ccamera::FrameBuffer buffer;
        ccamera::AppConfig config;
        ccamera::CameraCapture camera(config, buffer, [] { return std::make_unique<FailingReadSource>(); });

        camera.start();
        assert(wait_for_error(camera, "read frame"));
        camera.stop();
        const auto status = camera.status();
        assert(status.opened);
        assert(status.frames_captured == 0);
    }

    return 0;
}
