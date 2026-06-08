#include "face/OpenCvZooFaceFeatureExtractor.hpp"

#include "core/Math.hpp"

#include <stdexcept>
#include <vector>

namespace ccamera {

FaceEmbedding face_embedding_from_sface_feature(const cv::Mat& feature) {
    if (feature.empty() || feature.type() != CV_32F) {
        throw std::runtime_error("SFace feature must be a non-empty CV_32F matrix");
    }

    std::vector<float> values;
    values.reserve(feature.total() * static_cast<std::size_t>(feature.channels()));
    if (feature.isContinuous()) {
        const float* begin = feature.ptr<float>(0);
        values.assign(begin, begin + feature.total() * static_cast<std::size_t>(feature.channels()));
    } else {
        for (int row = 0; row < feature.rows; ++row) {
            const float* row_values = feature.ptr<float>(row);
            values.insert(values.end(), row_values, row_values + feature.cols * feature.channels());
        }
    }

    if (values.empty()) {
        throw std::runtime_error("SFace feature must contain at least one value");
    }

    return FaceEmbedding{l2_normalized(values)};
}

OpenCvZooFaceFeatureExtractor::OpenCvZooFaceFeatureExtractor(const std::string& model_path)
    : recognizer_(cv::FaceRecognizerSF::create(model_path, "")) {
    if (recognizer_.empty()) {
        throw std::runtime_error("failed to create OpenCV Zoo SFace recognizer");
    }
}

FaceEmbedding OpenCvZooFaceFeatureExtractor::extract(const cv::Mat& aligned_face) {
    if (aligned_face.empty()) {
        throw std::runtime_error("OpenCV Zoo recognizer input face is empty");
    }

    cv::Mat feature;
    recognizer_->feature(aligned_face, feature);
    return face_embedding_from_sface_feature(feature);
}

}  // namespace ccamera
