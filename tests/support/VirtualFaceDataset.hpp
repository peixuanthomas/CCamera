#pragma once

#include "core/Types.hpp"
#include "face/FaceAligner.hpp"
#include "face/FaceDatabase.hpp"
#include "face/FaceDetector.hpp"
#include "face/FaceFeatureExtractor.hpp"
#include "face/Recognizer.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ccamera::test_support {

struct VirtualFaceDataset {
    std::string identity_name;
    std::vector<Frame> frames;
    std::vector<float> detection_row;
    FaceEmbedding stored_embedding;
    Tensor recognizer_output;
};

inline VirtualFaceDataset make_single_face_virtual_dataset() {
    cv::Mat image = cv::Mat::zeros(160, 160, CV_8UC3);
    for (int y = 35; y < 125; ++y) {
        for (int x = 45; x < 115; ++x) {
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(24, 96, 180);
        }
    }

    Frame frame;
    frame.image = image;
    frame.sequence = 42;
    frame.captured_at = Clock::time_point{};

    return {
        "VirtualAlice",
        {frame},
        {
            45.0f, 35.0f, 115.0f, 125.0f, 0.97f,
            62.0f, 63.0f,
            96.0f, 63.0f,
            80.0f, 83.0f,
            68.0f, 108.0f,
            92.0f, 108.0f,
        },
        {{1.0f, 0.0f, 0.0f}},
        {{3}, {5.0f, 0.0f, 0.0f}},
    };
}

class VirtualDatasetDetectorEngine final : public InferenceEngine {
public:
    explicit VirtualDatasetDetectorEngine(const VirtualFaceDataset& dataset) : dataset_(dataset) {}

    TensorMap run(const TensorMap& inputs) override {
        ++run_count_;
        const auto found = inputs.find("image");
        if (found == inputs.end()) {
            throw std::runtime_error("virtual detector expected image input");
        }
        validate_tensor_shape_and_values("image", found->second);
        if (found->second.shape.size() != 4 || found->second.shape[0] != 1) {
            throw std::runtime_error("virtual detector expected a batch-1 NCHW image tensor");
        }
        if (dataset_.frames.empty()) {
            throw std::runtime_error("virtual detector dataset has no frames");
        }

        const Frame& frame = dataset_.frames.front();
        if (found->second.shape[1] != frame.image.channels() ||
            found->second.shape[2] != frame.image.rows ||
            found->second.shape[3] != frame.image.cols) {
            throw std::runtime_error("virtual detector image tensor shape does not match virtual frame");
        }

        return {{"detections", {{1, 15}, dataset_.detection_row}}};
    }

    int run_count() const {
        return run_count_;
    }

private:
    const VirtualFaceDataset& dataset_;
    int run_count_ = 0;
};

class VirtualDatasetRecognizerEngine final : public InferenceEngine {
public:
    explicit VirtualDatasetRecognizerEngine(const VirtualFaceDataset& dataset) : dataset_(dataset) {}

    TensorMap run(const TensorMap& inputs) override {
        ++run_count_;
        const auto found = inputs.find("face");
        if (found == inputs.end()) {
            throw std::runtime_error("virtual recognizer expected face input");
        }
        validate_tensor_shape_and_values("face", found->second);
        if (found->second.shape.size() != 4 || found->second.shape[0] != 1 ||
            found->second.shape[2] != 112 || found->second.shape[3] != 112) {
            throw std::runtime_error("virtual recognizer expected a batch-1 aligned face tensor");
        }

        return {{"embedding", dataset_.recognizer_output}};
    }

    int run_count() const {
        return run_count_;
    }

private:
    const VirtualFaceDataset& dataset_;
    int run_count_ = 0;
};

inline FaceDatabaseSnapshot make_virtual_database_snapshot(const VirtualFaceDataset& dataset) {
    FaceDatabaseSnapshot snapshot;
    snapshot.embedding_dimension = static_cast<int>(dataset.stored_embedding.values.size());
    snapshot.people.push_back({"virtual-alice", dataset.identity_name, {dataset.stored_embedding.values}});
    return snapshot;
}

inline std::vector<RecognitionResult> run_virtual_recognition_frame(
    const Frame& frame,
    FaceDetector& detector,
    const FaceAligner& aligner,
    FaceFeatureExtractor& extractor,
    const FaceDatabaseSnapshot& database,
    const Recognizer& recognizer) {
    std::vector<RecognitionResult> results;
    const std::vector<FaceDetection> faces = detector.detect(frame.image);
    for (const FaceDetection& face : faces) {
        const cv::Mat aligned_face = aligner.align(frame.image, face);
        const FaceEmbedding embedding = extractor.extract(aligned_face);
        const MatchResult match = recognizer.match(embedding, database);

        RecognitionResult result;
        result.detection = face;
        result.label = match.label;
        result.similarity = match.similarity;
        result.known = match.known;
        result.frame_sequence = frame.sequence;
        results.push_back(std::move(result));
    }
    return results;
}

}  // namespace ccamera::test_support
