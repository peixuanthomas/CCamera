#include "app/AppController.hpp"
#include "ui/PreviewWindow.hpp"

#include <cassert>
#include <filesystem>
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
    double,
    const std::string&) {}

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
    ccamera::AppConfig config;
    config.database_path = (std::filesystem::temp_directory_path() / "ccamera_web_commands_test.json").string();
    config.detector_model = "models/opencv_zoo/face_detection_yunet_2023mar.onnx";
    config.recognizer_model = "models/opencv_zoo/face_recognition_sface_2021dec.onnx";
    std::filesystem::remove(config.database_path);

    ccamera::AppController controller(config);
    bool running = true;

    controller.enqueue_web_command_for_test({ccamera::CommandType::Enroll, "Alice"});
    controller.enqueue_web_command_for_test({ccamera::CommandType::Quit, {}});
    controller.apply_commands_for_test(running);

    assert(!running);
    assert(controller.enrollment_active_for_test());
    assert(controller.status_message_for_test().find("Enrolling Alice") != std::string::npos);

    std::filesystem::remove(config.database_path);
    return 0;
}
