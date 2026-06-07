#include "app/AppConfig.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

template <typename Callable>
void expect_invalid_config(Callable&& callable, const std::string& expected_message) {
    try {
        callable();
    } catch (const std::invalid_argument& error) {
        assert(std::string(error.what()).find(expected_message) != std::string::npos);
        return;
    }
    assert(false);
}

std::filesystem::path write_config(const std::string& name, const std::string& json) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream out(path);
    out << json;
    return path;
}

}  // namespace

int main() {
    const auto example = ccamera::load_app_config("config/app.example.json");
    assert(example.camera_index == 0);
    assert(example.frame_width == 1280);
    assert(example.frame_height == 720);
    assert(example.inference_backend == "onnxruntime_cpu");
    assert(example.enrollment_consistency_threshold == 0.55f);

    const auto override_path = write_config(
        "ccamera_config_override.json",
        R"json({
          "camera_index": 2,
          "frame_width": 640,
          "frame_height": 480,
          "detector_model": "models/custom_detector.onnx",
          "recognizer_model": "models/custom_recognizer.onnx",
          "database_path": "data/custom_db.json",
          "inference_backend": "onnxruntime_cpu",
          "detection_interval": 2,
          "detection_threshold": 0.8,
          "recognition_threshold": 0.3,
          "enrollment_consistency_threshold": 0.9,
          "enrollment_sample_count": 3,
          "enrollment_timeout_seconds": 5,
          "feature_worker_count": 1
        })json");
    const auto override_config = ccamera::load_app_config(override_path.string());
    assert(override_config.camera_index == 2);
    assert(override_config.frame_width == 640);
    assert(override_config.detector_model == "models/custom_detector.onnx");
    assert(override_config.enrollment_sample_count == 3);

    const auto saved_path = std::filesystem::temp_directory_path() / "ccamera_saved_config.json";
    ccamera::save_app_config(saved_path.string(), override_config);
    const auto saved_config = ccamera::load_app_config(saved_path.string());
    assert(saved_config.camera_index == 2);
    assert(saved_config.frame_width == 640);
    assert(saved_config.frame_height == 480);
    assert(saved_config.detector_model == "models/custom_detector.onnx");
    assert(saved_config.recognizer_model == "models/custom_recognizer.onnx");
    assert(saved_config.database_path == "data/custom_db.json");
    assert(saved_config.detection_threshold == 0.8f);
    assert(saved_config.recognition_threshold == 0.3f);
    assert(saved_config.enrollment_sample_count == 3);

    const std::string override_path_text = override_path.string();
    const char* argv_with_flag[] = {"ccamera.exe", "--config", override_path_text.c_str()};
    const auto argv_config = ccamera::load_app_config_from_args(3, const_cast<char**>(argv_with_flag));
    assert(argv_config.frame_width == 640);

    expect_invalid_config(
        [] {
            const auto path = write_config("ccamera_bad_backend.json", R"json({"inference_backend":"mock"})json");
            (void)ccamera::load_app_config(path.string());
        },
        "inference_backend");

    expect_invalid_config(
        [] {
            const auto path = write_config("ccamera_bad_threshold.json", R"json({"detection_threshold":1.2})json");
            (void)ccamera::load_app_config(path.string());
        },
        "detection_threshold");

    expect_invalid_config(
        [] {
            ccamera::AppConfig config;
            config.enrollment_timeout_seconds = 0;
            ccamera::validate_app_config(config);
        },
        "enrollment_timeout_seconds");

    std::filesystem::remove(override_path);
    std::filesystem::remove(saved_path);
    return 0;
}
