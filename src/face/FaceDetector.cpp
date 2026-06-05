#include "face/FaceDetector.hpp"

#include "face/ModelContracts.hpp"

#include <stdexcept>

namespace ccamera {

FaceDetector::FaceDetector(InferenceEngine& engine, float threshold)
    : engine_(engine), threshold_(threshold) {}

std::vector<FaceDetection> FaceDetector::detect(const cv::Mat& frame) {
    if (frame.empty()) {
        throw std::runtime_error("detector input frame is empty");
    }
    if (frame.channels() <= 0) {
        throw std::runtime_error("detector input frame must have channels");
    }

    const auto contract = detector_model_contract();
    cv::Mat float_frame;
    frame.convertTo(float_frame, CV_32F);

    Tensor input;
    input.shape = {1, frame.channels(), frame.rows, frame.cols};
    input.values.resize(static_cast<std::size_t>(frame.total() * frame.channels()));
    const int channels = frame.channels();
    const int frame_rows = frame.rows;
    const int frame_cols = frame.cols;
    const auto plane_size = static_cast<std::size_t>(frame_rows * frame_cols);
    for (int y = 0; y < frame_rows; ++y) {
        const float* row = float_frame.ptr<float>(y);
        for (int x = 0; x < frame_cols; ++x) {
            for (int channel = 0; channel < channels; ++channel) {
                const auto output_index =
                    static_cast<std::size_t>(channel) * plane_size + static_cast<std::size_t>(y * frame_cols + x);
                input.values[output_index] = row[x * channels + channel];
            }
        }
    }

    validate_tensor_shape_and_values(contract.input_name, input);

    const auto outputs = engine_.run({{contract.input_name, input}});
    const auto found = outputs.find(contract.output_name);
    if (found == outputs.end()) {
        throw std::runtime_error("detector output '" + contract.output_name + "' is missing");
    }

    const Tensor& tensor = found->second;
    if (tensor.shape.size() != 2 || tensor.shape[0] < 0 || tensor.shape[1] != contract.detection_columns) {
        throw std::runtime_error("detector output must have shape [N, 15]");
    }

    const int64_t rows = tensor.shape[0];
    const auto expected_values = static_cast<std::size_t>(rows * contract.detection_columns);
    if (tensor.values.size() != expected_values) {
        throw std::runtime_error("detector output values do not match shape [N, 15]");
    }

    std::vector<FaceDetection> faces;
    for (int64_t row = 0; row < rows; ++row) {
        const auto base = static_cast<std::size_t>(row * 15);
        const float score = tensor.values[base + 4];
        if (score < threshold_) {
            continue;
        }

        FaceDetection face;
        const float x1 = tensor.values[base + 0];
        const float y1 = tensor.values[base + 1];
        const float x2 = tensor.values[base + 2];
        const float y2 = tensor.values[base + 3];
        face.box = cv::Rect2f(cv::Point2f(x1, y1), cv::Point2f(x2, y2));
        face.score = score;
        face.keypoints.left_eye = cv::Point2f(tensor.values[base + 5], tensor.values[base + 6]);
        face.keypoints.right_eye = cv::Point2f(tensor.values[base + 7], tensor.values[base + 8]);
        face.keypoints.nose = cv::Point2f(tensor.values[base + 9], tensor.values[base + 10]);
        face.keypoints.mouth_left = cv::Point2f(tensor.values[base + 11], tensor.values[base + 12]);
        face.keypoints.mouth_right = cv::Point2f(tensor.values[base + 13], tensor.values[base + 14]);
        faces.push_back(face);
    }

    return faces;
}

}  // namespace ccamera
