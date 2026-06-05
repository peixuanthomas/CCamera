#include "camera/CameraCapture.hpp"

#include <cassert>
#include <atomic>
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

class ImmediateFailingReadSource final : public ccamera::CameraSource {
public:
    explicit ImmediateFailingReadSource(std::atomic<int>& read_count) : read_count_(read_count) {}

    bool open(int) override { return true; }
    bool is_opened() const override { return true; }
    void set(int, double) override {}
    bool read(cv::Mat&) override {
        ++read_count_;
        return false;
    }

private:
    std::atomic<int>& read_count_;
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

    {
        ccamera::FrameBuffer buffer;
        ccamera::AppConfig config;
        std::atomic<int> read_count{0};
        ccamera::CameraCapture camera(config, buffer, [&] {
            return std::make_unique<ImmediateFailingReadSource>(read_count);
        });

        camera.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        camera.stop();

        assert(read_count.load() <= 20);
        assert(camera.status().last_error.find("read frame") != std::string::npos);
    }

    return 0;
}
