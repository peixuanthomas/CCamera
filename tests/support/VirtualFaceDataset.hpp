#pragma once

#include "core/Math.hpp"
#include "core/Types.hpp"
#include "face/FaceAligner.hpp"
#include "face/FaceDatabase.hpp"
#include "face/FaceDetector.hpp"
#include "face/FaceFeatureExtractor.hpp"
#include "face/Recognizer.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

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
    cv::Rect2f expected_box;
    FaceKeypoints expected_keypoints;
    Tensor expected_recognizer_output;
    FaceEmbedding stored_embedding;
};

inline Tensor bgr_mean_embedding_from_aligned_face(const cv::Mat& aligned_face) {
    if (aligned_face.empty() || aligned_face.channels() != 3) {
        throw std::runtime_error("virtual aligned face must be a non-empty 3-channel image");
    }

    std::vector<float> sums(3, 0.0f);
    for (int y = 0; y < aligned_face.rows; ++y) {
        for (int x = 0; x < aligned_face.cols; ++x) {
            const cv::Vec3b pixel = aligned_face.at<cv::Vec3b>(y, x);
            sums[0] += static_cast<float>(pixel[0]);
            sums[1] += static_cast<float>(pixel[1]);
            sums[2] += static_cast<float>(pixel[2]);
        }
    }

    const float count = static_cast<float>(aligned_face.rows * aligned_face.cols);
    return {{3}, {sums[0] / count, sums[1] / count, sums[2] / count}};
}

inline Tensor bgr_mean_embedding_from_face_tensor(const Tensor& tensor) {
    if (tensor.shape.size() != 4 || tensor.shape[0] != 1 || tensor.shape[1] != 3) {
        throw std::runtime_error("virtual recognizer expected a batch-1 3-channel aligned face tensor");
    }
    validate_tensor_shape_and_values("face", tensor);

    const auto rows = static_cast<std::size_t>(tensor.shape[2]);
    const auto cols = static_cast<std::size_t>(tensor.shape[3]);
    const auto plane_size = rows * cols;
    std::vector<float> means(3, 0.0f);
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const auto offset = channel * plane_size;
        for (std::size_t i = 0; i < plane_size; ++i) {
            means[channel] += tensor.values[offset + i];
        }
        means[channel] /= static_cast<float>(plane_size);
    }

    return {{3}, std::move(means)};
}

inline VirtualFaceDataset make_single_face_virtual_dataset() {
    cv::Mat image = cv::Mat::zeros(160, 160, CV_8UC3);
    for (int y = 0; y < image.rows; ++y) {
        for (int x = 0; x < image.cols; ++x) {
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(
                static_cast<unsigned char>(x),
                static_cast<unsigned char>(y),
                static_cast<unsigned char>((x + y) / 2));
        }
    }

    Frame frame;
    frame.image = image;
    frame.sequence = 42;
    frame.captured_at = Clock::time_point{};

    cv::Mat expected_aligned_face;
    cv::resize(image(cv::Rect(35, 22, 90, 116)), expected_aligned_face, cv::Size(112, 112));
    const Tensor expected_recognizer_output = bgr_mean_embedding_from_aligned_face(expected_aligned_face);

    FaceKeypoints expected_keypoints;
    expected_keypoints.left_eye = cv::Point2f(62.0f, 63.0f);
    expected_keypoints.right_eye = cv::Point2f(96.0f, 63.0f);
    expected_keypoints.nose = cv::Point2f(80.0f, 83.0f);
    expected_keypoints.mouth_left = cv::Point2f(68.0f, 108.0f);
    expected_keypoints.mouth_right = cv::Point2f(92.0f, 108.0f);

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
        cv::Rect2f(45.0f, 35.0f, 70.0f, 90.0f),
        expected_keypoints,
        expected_recognizer_output,
        {l2_normalized(expected_recognizer_output.values)},
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
        const auto rows = static_cast<std::size_t>(frame.image.rows);
        const auto cols = static_cast<std::size_t>(frame.image.cols);
        const auto sentinel_index = static_cast<std::size_t>(67) * cols + static_cast<std::size_t>(101);
        const cv::Vec3b sentinel = frame.image.at<cv::Vec3b>(67, 101);
        if (found->second.values[sentinel_index] != static_cast<float>(sentinel[0]) ||
            found->second.values[rows * cols + sentinel_index] != static_cast<float>(sentinel[1]) ||
            found->second.values[2 * rows * cols + sentinel_index] != static_cast<float>(sentinel[2])) {
            throw std::runtime_error("virtual detector image tensor values do not match virtual frame pixels");
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

        last_raw_embedding_ = bgr_mean_embedding_from_face_tensor(found->second);
        return {{"embedding", last_raw_embedding_}};
    }

    int run_count() const {
        return run_count_;
    }

    const std::vector<float>& last_raw_embedding() const {
        return last_raw_embedding_.values;
    }

private:
    const VirtualFaceDataset& dataset_;
    int run_count_ = 0;
    Tensor last_raw_embedding_;
};

inline FaceDatabaseSnapshot make_virtual_database_snapshot(const VirtualFaceDataset& dataset) {
    FaceDatabaseSnapshot snapshot;
    snapshot.embedding_dimension = static_cast<int>(dataset.stored_embedding.values.size());
    snapshot.people.push_back({"virtual-alice", dataset.identity_name, {dataset.stored_embedding.values}});
    return snapshot;
}

}  // namespace ccamera::test_support
