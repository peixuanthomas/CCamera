#pragma once

#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ccamera {

using Clock = std::chrono::steady_clock;

struct Frame {
    cv::Mat image;
    std::uint64_t sequence = 0;
    Clock::time_point captured_at{};
};

struct FaceKeypoints {
    std::optional<cv::Point2f> left_eye;
    std::optional<cv::Point2f> right_eye;
    std::optional<cv::Point2f> nose;
    std::optional<cv::Point2f> mouth_left;
    std::optional<cv::Point2f> mouth_right;
};

struct FaceDetection {
    cv::Rect2f box;
    float score = 0.0f;
    FaceKeypoints keypoints;
};

struct FaceEmbedding {
    std::vector<float> values;
};

struct RecognitionResult {
    FaceDetection detection;
    std::string label = "Unknown";
    float similarity = 0.0f;
    bool known = false;
    std::uint64_t frame_sequence = 0;
};

struct CameraStatus {
    bool running = false;
    bool opened = false;
    std::uint64_t frames_captured = 0;
    std::string last_error;
};

struct AppConfig {
    int camera_index = 0;
    int frame_width = 1280;
    int frame_height = 720;
    std::string detector_model = "models/face_detector.onnx";
    std::string recognizer_model = "models/face_recognizer.onnx";
    std::string database_path = "data/face_db.json";
    std::string inference_backend = "onnxruntime_cpu";
    int detection_interval = 3;
    float detection_threshold = 0.7f;
    float recognition_threshold = 0.45f;
    float enrollment_consistency_threshold = 0.55f;
    int enrollment_sample_count = 15;
    int enrollment_timeout_seconds = 20;
    int feature_worker_count = 2;
};

}  // namespace ccamera
