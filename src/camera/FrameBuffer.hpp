#pragma once

#include "core/Types.hpp"

#include <mutex>
#include <optional>

namespace ccamera {

class FrameBuffer {
public:
    void publish(const Frame& frame);
    std::optional<Frame> latest() const;

private:
    mutable std::mutex mutex_;
    std::optional<Frame> latest_;
};

}  // namespace ccamera
