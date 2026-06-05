#pragma once

#include "camera/FrameBuffer.hpp"
#include "core/Types.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace ccamera {

class CameraSource {
public:
    virtual ~CameraSource() = default;
    virtual bool open(int camera_index) = 0;
    virtual bool is_opened() const = 0;
    virtual void set(int property, double value) = 0;
    virtual bool read(cv::Mat& image) = 0;
};

using CameraSourceFactory = std::function<std::unique_ptr<CameraSource>()>;

class CameraCapture {
public:
    CameraCapture(AppConfig config, FrameBuffer& buffer);
    CameraCapture(AppConfig config, FrameBuffer& buffer, CameraSourceFactory source_factory);
    ~CameraCapture();

    void start();
    void stop();
    CameraStatus status() const;

private:
    void run();
    void update_status_opened(bool opened);
    void update_status_error(const std::string& error);
    void update_status_frame_captured();

    AppConfig config_;
    FrameBuffer& buffer_;
    CameraSourceFactory source_factory_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    mutable std::mutex status_mutex_;
    CameraStatus status_;
};

}  // namespace ccamera
