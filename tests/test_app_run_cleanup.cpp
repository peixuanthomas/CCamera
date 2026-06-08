#include "app/AppController.hpp"
#include "app/AppRunCleanup.hpp"
#include "ui/PreviewWindow.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>

namespace cv {

int waitKey(int) {
    return -1;
}

}  // namespace cv

namespace ccamera {

PreviewWindow::PreviewWindow(std::string name) : name_(std::move(name)) {}

void PreviewWindow::show(
    const cv::Mat&,
    const std::vector<RecognitionResult>&,
    const FaceDatabaseSnapshot&,
    double) {}

std::vector<Command> PreviewWindow::consume_commands() {
    return {};
}

std::vector<Command> PreviewWindow::handle_key(int) {
    return {};
}

bool PreviewWindow::accepting_text_input() const {
    return false;
}

}  // namespace ccamera

int main() {
    bool recognition_stopped = false;
    bool command_stopped = false;
    bool camera_stopped = false;

    try {
        ccamera::AppRunCleanup cleanup(
            [&] { recognition_stopped = true; },
            [&] { command_stopped = true; },
            [&] { camera_stopped = true; });
        throw std::runtime_error("preview loop failed");
    } catch (const std::runtime_error&) {
    }

    assert(recognition_stopped);
    assert(command_stopped);
    assert(camera_stopped);

    recognition_stopped = false;
    command_stopped = false;
    camera_stopped = false;

    {
        ccamera::AppRunCleanup cleanup(
            [&] { recognition_stopped = true; },
            [&] { command_stopped = true; },
            [&] { camera_stopped = true; });
        cleanup.dismiss();
    }

    assert(!recognition_stopped);
    assert(!command_stopped);
    assert(!camera_stopped);

    bool saw_missing_model_error = false;
    {
        ccamera::AppConfig config;
        config.inference_backend = "opencv_zoo";
        config.detector_model = "models/missing-yunet.onnx";
        config.recognizer_model = "models/missing-sface.onnx";
        try {
            ccamera::AppController app(config);
            (void)app;
        } catch (const std::exception& error) {
            const std::string message = error.what();
            saw_missing_model_error =
                message.find("missing-yunet.onnx") != std::string::npos ||
                message.find("missing-sface.onnx") != std::string::npos;
        }
    }
    assert(saw_missing_model_error);

    return 0;
}
