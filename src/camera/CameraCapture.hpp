#pragma once

#include "camera/FrameBuffer.hpp"
#include "core/Types.hpp"

#include <atomic>
#include <thread>

namespace ccamera {

class CameraCapture {
public:
    CameraCapture(AppConfig config, FrameBuffer& buffer);
    ~CameraCapture();

    void start();
    void stop();

private:
    void run();

    AppConfig config_;
    FrameBuffer& buffer_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace ccamera
