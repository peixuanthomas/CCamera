#include "app/AppConfig.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace ccamera {
namespace {

template <typename T>
void set_if_present(const nlohmann::json& json, const char* key, T& target) {
    if (json.contains(key)) {
        target = json.at(key).get<T>();
    }
}

void require_positive(const char* name, int value) {
    if (value <= 0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
}

void require_probability(const char* name, float value) {
    if (value < 0.0f || value > 1.0f) {
        throw std::invalid_argument(std::string(name) + " must be between 0 and 1");
    }
}

void require_non_empty(const char* name, const std::string& value) {
    if (value.empty()) {
        throw std::invalid_argument(std::string(name) + " must not be empty");
    }
}

}  // namespace

void validate_app_config(const AppConfig& config) {
    if (config.camera_index < 0) {
        throw std::invalid_argument("camera_index must be non-negative");
    }
    require_positive("frame_width", config.frame_width);
    require_positive("frame_height", config.frame_height);
    require_non_empty("detector_model", config.detector_model);
    require_non_empty("recognizer_model", config.recognizer_model);
    require_non_empty("database_path", config.database_path);
    require_non_empty("database_passphrase", config.database_passphrase);
    if (config.inference_backend != "onnxruntime_cpu" && config.inference_backend != "opencv_zoo") {
        throw std::invalid_argument("unknown inference_backend: " + config.inference_backend);
    }
    if (config.recognizer_backend != "onnxruntime_cpu" && config.recognizer_backend != "opencv_zoo" &&
        config.recognizer_backend != "insightface_arcface") {
        throw std::invalid_argument("unknown recognizer_backend: " + config.recognizer_backend);
    }
    require_positive("detection_interval", config.detection_interval);
    require_probability("detection_threshold", config.detection_threshold);
    require_probability("recognition_threshold", config.recognition_threshold);
    require_probability("enrollment_consistency_threshold", config.enrollment_consistency_threshold);
    require_positive("enrollment_sample_count", config.enrollment_sample_count);
    require_positive("enrollment_timeout_seconds", config.enrollment_timeout_seconds);
    require_positive("feature_worker_count", config.feature_worker_count);
}

AppConfig load_app_config(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open app config: " + path);
    }

    nlohmann::json json;
    in >> json;

    AppConfig config;
    set_if_present(json, "camera_index", config.camera_index);
    set_if_present(json, "frame_width", config.frame_width);
    set_if_present(json, "frame_height", config.frame_height);
    set_if_present(json, "detector_model", config.detector_model);
    set_if_present(json, "recognizer_model", config.recognizer_model);
    set_if_present(json, "database_path", config.database_path);
    set_if_present(json, "database_passphrase", config.database_passphrase);
    set_if_present(json, "inference_backend", config.inference_backend);
    if (json.contains("recognizer_backend")) {
        config.recognizer_backend = json.at("recognizer_backend").get<std::string>();
    } else {
        config.recognizer_backend = config.inference_backend;
    }
    set_if_present(json, "detection_interval", config.detection_interval);
    set_if_present(json, "detection_threshold", config.detection_threshold);
    set_if_present(json, "recognition_threshold", config.recognition_threshold);
    set_if_present(json, "enrollment_consistency_threshold", config.enrollment_consistency_threshold);
    set_if_present(json, "enrollment_sample_count", config.enrollment_sample_count);
    set_if_present(json, "enrollment_timeout_seconds", config.enrollment_timeout_seconds);
    set_if_present(json, "feature_worker_count", config.feature_worker_count);

    validate_app_config(config);
    return config;
}

void save_app_config(const std::string& path, const AppConfig& config) {
    validate_app_config(config);

    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    nlohmann::json json;
    json["camera_index"] = config.camera_index;
    json["frame_width"] = config.frame_width;
    json["frame_height"] = config.frame_height;
    json["detector_model"] = config.detector_model;
    json["recognizer_model"] = config.recognizer_model;
    json["database_path"] = config.database_path;
    json["database_passphrase"] = config.database_passphrase;
    json["inference_backend"] = config.inference_backend;
    json["recognizer_backend"] = config.recognizer_backend;
    json["detection_interval"] = config.detection_interval;
    json["detection_threshold"] = config.detection_threshold;
    json["recognition_threshold"] = config.recognition_threshold;
    json["enrollment_consistency_threshold"] = config.enrollment_consistency_threshold;
    json["enrollment_sample_count"] = config.enrollment_sample_count;
    json["enrollment_timeout_seconds"] = config.enrollment_timeout_seconds;
    json["feature_worker_count"] = config.feature_worker_count;

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open app config for writing: " + path);
    }
    out << json.dump(2) << '\n';
}

AppConfig load_app_config_from_args(int argc, char** argv) {
    std::string config_path;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--config") {
            if (index + 1 >= argc) {
                throw std::invalid_argument("--config requires a path");
            }
            config_path = argv[++index];
        } else if (arg.rfind("--config=", 0) == 0) {
            config_path = arg.substr(std::string("--config=").size());
        } else if (config_path.empty() && arg.rfind("-", 0) != 0) {
            config_path = arg;
        } else {
            throw std::invalid_argument("unknown argument: " + arg);
        }
    }

    if (config_path.empty()) {
        AppConfig config;
        validate_app_config(config);
        return config;
    }
    return load_app_config(config_path);
}

}  // namespace ccamera
