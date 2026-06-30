#include "face/InsightFaceArcFaceFeatureExtractor.hpp"

#include "core/Math.hpp"
#include "face/FaceAligner.hpp"
#include "face/ModelContracts.hpp"

#include <opencv2/imgproc.hpp>

#include <array>
#include <stdexcept>
#include <utility>

namespace ccamera {
namespace {

using LandmarkSet = std::array<cv::Point2f, 5>;

LandmarkSet arcface_reference_landmarks() {
    return {
        cv::Point2f{38.2946f, 51.6963f},
        cv::Point2f{73.5318f, 51.5014f},
        cv::Point2f{56.0252f, 71.7366f},
        cv::Point2f{41.5493f, 92.3655f},
        cv::Point2f{70.7299f, 92.2041f},
    };
}

LandmarkSet require_arcface_landmarks(const FaceDetection& detection) {
    const FaceKeypoints& keypoints = detection.keypoints;
    if (!keypoints.left_eye || !keypoints.right_eye || !keypoints.nose ||
        !keypoints.mouth_left || !keypoints.mouth_right) {
        throw std::runtime_error(
            "InsightFace ArcFace alignment requires five landmarks: "
            "left_eye, right_eye, nose, mouth_left, mouth_right");
    }

    return {
        *keypoints.left_eye,
        *keypoints.right_eye,
        *keypoints.nose,
        *keypoints.mouth_left,
        *keypoints.mouth_right,
    };
}

cv::Mat similarity_transform(const LandmarkSet& source, const LandmarkSet& target) {
    cv::Point2d source_mean{0.0, 0.0};
    cv::Point2d target_mean{0.0, 0.0};
    for (std::size_t i = 0; i < source.size(); ++i) {
        source_mean.x += source[i].x;
        source_mean.y += source[i].y;
        target_mean.x += target[i].x;
        target_mean.y += target[i].y;
    }
    source_mean.x /= static_cast<double>(source.size());
    source_mean.y /= static_cast<double>(source.size());
    target_mean.x /= static_cast<double>(target.size());
    target_mean.y /= static_cast<double>(target.size());

    double denominator = 0.0;
    double a_numerator = 0.0;
    double b_numerator = 0.0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        const double x = static_cast<double>(source[i].x) - source_mean.x;
        const double y = static_cast<double>(source[i].y) - source_mean.y;
        const double u = static_cast<double>(target[i].x) - target_mean.x;
        const double v = static_cast<double>(target[i].y) - target_mean.y;

        denominator += x * x + y * y;
        a_numerator += x * u + y * v;
        b_numerator += x * v - y * u;
    }

    if (denominator <= 1.0e-12) {
        throw std::runtime_error("InsightFace ArcFace landmarks are degenerate");
    }

    const double a = a_numerator / denominator;
    const double b = b_numerator / denominator;
    const double tx = target_mean.x - a * source_mean.x + b * source_mean.y;
    const double ty = target_mean.y - b * source_mean.x - a * source_mean.y;

    cv::Mat transform(2, 3, CV_64F);
    transform.at<double>(0, 0) = a;
    transform.at<double>(0, 1) = -b;
    transform.at<double>(0, 2) = tx;
    transform.at<double>(1, 0) = b;
    transform.at<double>(1, 1) = a;
    transform.at<double>(1, 2) = ty;
    return transform;
}

cv::Mat align_with_arcface_landmarks(const cv::Mat& frame, const FaceDetection& detection) {
    if (frame.empty()) {
        throw std::runtime_error("InsightFace ArcFace input frame is empty");
    }

    const LandmarkSet source = require_arcface_landmarks(detection);
    const cv::Mat transform = similarity_transform(source, arcface_reference_landmarks());

    cv::Mat aligned;
    cv::warpAffine(
        frame,
        aligned,
        transform,
        cv::Size{
            insightface_arcface_model_contract().input_width,
            insightface_arcface_model_contract().input_height},
        cv::INTER_LINEAR,
        cv::BORDER_CONSTANT,
        cv::Scalar{0, 0, 0});
    if (aligned.empty()) {
        throw std::runtime_error("InsightFace ArcFace alignment produced an empty crop");
    }
    return aligned;
}

Tensor make_arcface_input_tensor(const cv::Mat& aligned_face) {
    if (aligned_face.empty()) {
        throw std::runtime_error("InsightFace ArcFace input face is empty");
    }
    if (aligned_face.channels() != 3) {
        throw std::runtime_error("InsightFace ArcFace input face must be a BGR image with 3 channels");
    }

    cv::Mat resized;
    const auto contract = insightface_arcface_model_contract();
    cv::resize(aligned_face, resized, cv::Size{contract.input_width, contract.input_height});

    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

    cv::Mat float_rgb;
    rgb.convertTo(float_rgb, CV_32F);

    Tensor input;
    input.shape = {1, 3, contract.input_height, contract.input_width};
    input.values.resize(static_cast<std::size_t>(3 * contract.input_width * contract.input_height));

    const auto plane_size = static_cast<std::size_t>(contract.input_width * contract.input_height);
    for (int y = 0; y < contract.input_height; ++y) {
        const cv::Vec3f* row = float_rgb.ptr<cv::Vec3f>(y);
        for (int x = 0; x < contract.input_width; ++x) {
            const auto pixel_index = static_cast<std::size_t>(y * contract.input_width + x);
            for (int channel = 0; channel < 3; ++channel) {
                input.values[static_cast<std::size_t>(channel) * plane_size + pixel_index] =
                    (row[x][channel] - 127.5f) / 127.5f;
            }
        }
    }

    return input;
}

void validate_arcface_embedding_output(const std::string& output_name, const Tensor& tensor) {
    const auto contract = insightface_arcface_model_contract();
    const bool has_valid_shape =
        (tensor.shape == std::vector<int64_t>{static_cast<int64_t>(contract.embedding_dimensions)}) ||
        (tensor.shape == std::vector<int64_t>{1, static_cast<int64_t>(contract.embedding_dimensions)});
    if (!has_valid_shape) {
        throw std::runtime_error(
            "InsightFace ArcFace output '" + output_name + "' must have shape [512] or [1,512]");
    }
    if (tensor.values.size() != static_cast<std::size_t>(contract.embedding_dimensions)) {
        throw std::runtime_error(
            "InsightFace ArcFace output '" + output_name + "' value count does not match shape");
    }
}

}  // namespace

InsightFaceArcFaceFeatureExtractor::InsightFaceArcFaceFeatureExtractor(
    InferenceEngine& engine,
    std::string input_name,
    std::string output_name)
    : engine_(engine),
      input_name_(input_name.empty() ? insightface_arcface_model_contract().input_name : std::move(input_name)),
      output_name_(output_name.empty() ? insightface_arcface_model_contract().output_name : std::move(output_name)) {}

FaceEmbedding InsightFaceArcFaceFeatureExtractor::extract(const cv::Mat& aligned_face) {
    Tensor input = make_arcface_input_tensor(aligned_face);

    const TensorMap outputs = engine_.run({{input_name_, std::move(input)}});
    const auto found = outputs.find(output_name_);
    if (found == outputs.end()) {
        throw std::runtime_error("InsightFace ArcFace output '" + output_name_ + "' is missing");
    }

    validate_arcface_embedding_output(output_name_, found->second);
    return FaceEmbedding{l2_normalized(found->second.values)};
}

FaceEmbedding InsightFaceArcFaceFeatureExtractor::embed(
    const cv::Mat& frame,
    const FaceDetection& detection,
    const FaceAligner& aligner) {
    (void)aligner;
    return extract(align_with_arcface_landmarks(frame, detection));
}

}  // namespace ccamera
