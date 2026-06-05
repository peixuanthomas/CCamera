#include "camera/CameraCapture.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>

namespace ccamera {
namespace {

constexpr int kOpenCvFrameWidthProperty = 3;
constexpr int kOpenCvFrameHeightProperty = 4;
constexpr auto kReadFailureBackoff = std::chrono::milliseconds(5);

}  // namespace

CameraCapture::CameraCapture(AppConfig config, FrameBuffer& buffer, CameraSourceFactory source_factory)
    : config_(std::move(config)), buffer_(buffer), source_factory_(std::move(source_factory)) {}

CameraCapture::~CameraCapture() {
    stop();
}

void CameraCapture::start() {
    if (running_.exchange(true)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        status_.running = true;
        status_.last_error.clear();
    }
    thread_ = std::thread(&CameraCapture::run, this);
}

void CameraCapture::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.running = false;
}

CameraStatus CameraCapture::status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    CameraStatus copy = status_;
    copy.running = running_;
    return copy;
}

void CameraCapture::update_status_opened(bool opened) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.opened = opened;
    status_.running = running_;
}

void CameraCapture::update_status_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.last_error = error;
    status_.running = running_;
}

void CameraCapture::update_status_frame_captured() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    ++status_.frames_captured;
    status_.last_error.clear();
    status_.running = running_;
}

void CameraCapture::run() {
    std::unique_ptr<CameraSource> capture = source_factory_ ? source_factory_() : nullptr;
    if (!capture || !capture->open(config_.camera_index) || !capture->is_opened()) {
        update_status_opened(false);
        update_status_error("failed to open camera index " + std::to_string(config_.camera_index));
        running_ = false;
        return;
    }
    update_status_opened(true);

    capture->set(kOpenCvFrameWidthProperty, config_.frame_width);
    capture->set(kOpenCvFrameHeightProperty, config_.frame_height);

    std::uint64_t sequence = 0;
    while (running_) {
        cv::Mat image;
        if (!capture->read(image) || image.empty()) {
            update_status_error("failed to read frame from camera index " + std::to_string(config_.camera_index));
            std::this_thread::sleep_for(kReadFailureBackoff);
            continue;
        }

        Frame frame;
        frame.image = image;
        frame.sequence = ++sequence;
        frame.captured_at = Clock::now();
        buffer_.publish(frame);
        update_status_frame_captured();
    }
}

}  // namespace ccamera
