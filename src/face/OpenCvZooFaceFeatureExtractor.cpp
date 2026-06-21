#include "face/OpenCvZooFaceFeatureExtractor.hpp"

#include "core/Math.hpp"
#include "face/FaceAligner.hpp"

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

FaceEmbedding OpenCvZooFaceFeatureExtractor::embed(
    const cv::Mat& frame,
    const FaceDetection& detection,
    const FaceAligner& aligner) {
    if (frame.empty()) {
        throw std::runtime_error("OpenCV Zoo recognizer input frame is empty");
    }

    // SFace expects a similarity-transform alignment from the five facial landmarks.
    // Fall back to the bounding-box aligner only when landmarks are unavailable.
    const FaceKeypoints& keypoints = detection.keypoints;
    if (!keypoints.right_eye || !keypoints.left_eye || !keypoints.nose ||
        !keypoints.mouth_right || !keypoints.mouth_left) {
        return extract(aligner.align(frame, detection));
    }

    // Reconstruct the YuNet detection row layout that FaceRecognizerSF::alignCrop reads:
    // [x, y, w, h, right_eye, left_eye, nose, right_mouth, left_mouth, score].
    cv::Mat face_row(1, 15, CV_32F);
    float* row = face_row.ptr<float>(0);
    row[0] = detection.box.x;
    row[1] = detection.box.y;
    row[2] = detection.box.width;
    row[3] = detection.box.height;
    row[4] = keypoints.right_eye->x;
    row[5] = keypoints.right_eye->y;
    row[6] = keypoints.left_eye->x;
    row[7] = keypoints.left_eye->y;
    row[8] = keypoints.nose->x;
    row[9] = keypoints.nose->y;
    row[10] = keypoints.mouth_right->x;
    row[11] = keypoints.mouth_right->y;
    row[12] = keypoints.mouth_left->x;
    row[13] = keypoints.mouth_left->y;
    row[14] = detection.score;

    cv::Mat aligned;
    recognizer_->alignCrop(frame, face_row, aligned);
    if (aligned.empty()) {
        throw std::runtime_error("OpenCV Zoo recognizer alignment produced an empty crop");
    }

    cv::Mat feature;
    recognizer_->feature(aligned, feature);
    return face_embedding_from_sface_feature(feature);
}

}  // namespace ccamera
