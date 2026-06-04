#include "camera/FrameBuffer.hpp"

namespace ccamera {

void FrameBuffer::publish(const Frame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = frame;
    latest_->image = frame.image.clone();
}

std::optional<Frame> FrameBuffer::latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!latest_.has_value()) {
        return std::nullopt;
    }
    Frame copy = *latest_;
    copy.image = latest_->image.clone();
    return copy;
}

}  // namespace ccamera
