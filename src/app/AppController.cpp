#include "app/AppController.hpp"

#include "camera/CameraCapture.hpp"
#include "ui/PreviewWindow.hpp"

#include <opencv2/highgui.hpp>

#include <utility>

namespace ccamera {

AppController::AppController(AppConfig config) : config_(std::move(config)) {}

int AppController::run() {
    CameraCapture camera(config_, frame_buffer_);
    PreviewWindow preview("CCamera");

    camera.start();

    bool running = true;
    while (running) {
        const auto frame = frame_buffer_.latest();
        if (frame.has_value()) {
            preview.show(frame->image, {}, 0.0);
        }

        const int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {
            running = false;
        }
    }

    camera.stop();
    return 0;
}

}  // namespace ccamera
