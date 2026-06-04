#pragma once

#include "camera/FrameBuffer.hpp"
#include "core/Types.hpp"

namespace ccamera {

class AppController {
public:
    explicit AppController(AppConfig config);
    int run();

private:
    AppConfig config_;
    FrameBuffer frame_buffer_;
};

}  // namespace ccamera
