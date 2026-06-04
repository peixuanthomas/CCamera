#include "camera/CameraCapture.hpp"

#include <opencv2/videoio.hpp>

#include <cstdint>
#include <utility>

namespace ccamera {

CameraCapture::CameraCapture(AppConfig config, FrameBuffer& buffer)
    : config_(std::move(config)), buffer_(buffer) {}

CameraCapture::~CameraCapture() {
    stop();
}

void CameraCapture::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&CameraCapture::run, this);
}

void CameraCapture::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void CameraCapture::run() {
    cv::VideoCapture capture(config_.camera_index);
    if (!capture.isOpened()) {
        running_ = false;
        return;
    }

    capture.set(cv::CAP_PROP_FRAME_WIDTH, config_.frame_width);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, config_.frame_height);

    std::uint64_t sequence = 0;
    while (running_) {
        cv::Mat image;
        if (!capture.read(image) || image.empty()) {
            continue;
        }

        Frame frame;
        frame.image = image;
        frame.sequence = ++sequence;
        frame.captured_at = Clock::now();
        buffer_.publish(frame);
    }
}

}  // namespace ccamera
