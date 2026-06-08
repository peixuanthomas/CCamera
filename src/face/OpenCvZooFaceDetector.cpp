#include "face/OpenCvZooFaceDetector.hpp"

#include <stdexcept>

namespace ccamera {

FaceDetection face_detection_from_yunet_row(const cv::Mat& detections, int row_index) {
    if (detections.empty() || detections.cols != 15 || detections.type() != CV_32F) {
        throw std::runtime_error("YuNet detection output must be a non-empty CV_32F matrix with 15 columns");
    }
    if (row_index < 0 || row_index >= detections.rows) {
        throw std::runtime_error("YuNet detection row index is out of range");
    }

    const float* row = detections.ptr<float>(row_index);
    FaceDetection face;
    face.box = cv::Rect2f(row[0], row[1], row[2], row[3]);
    face.keypoints.right_eye = cv::Point2f(row[4], row[5]);
    face.keypoints.left_eye = cv::Point2f(row[6], row[7]);
    face.keypoints.nose = cv::Point2f(row[8], row[9]);
    face.keypoints.mouth_right = cv::Point2f(row[10], row[11]);
    face.keypoints.mouth_left = cv::Point2f(row[12], row[13]);
    face.score = row[14];
    return face;
}

OpenCvZooFaceDetector::OpenCvZooFaceDetector(const std::string& model_path, float threshold)
    : detector_(cv::FaceDetectorYN::create(model_path, "", cv::Size(320, 320), threshold, 0.3f, 5000)),
      input_size_(320, 320) {
    if (detector_.empty()) {
        throw std::runtime_error("failed to create OpenCV Zoo YuNet detector");
    }
}

std::vector<FaceDetection> OpenCvZooFaceDetector::detect(const cv::Mat& frame) {
    if (frame.empty()) {
        throw std::runtime_error("OpenCV Zoo detector input frame is empty");
    }

    const cv::Size frame_size(frame.cols, frame.rows);
    if (frame_size != input_size_) {
        detector_->setInputSize(frame_size);
        input_size_ = frame_size;
    }

    cv::Mat detections;
    detector_->detect(frame, detections);

    std::vector<FaceDetection> faces;
    for (int row = 0; row < detections.rows; ++row) {
        faces.push_back(face_detection_from_yunet_row(detections, row));
    }
    return faces;
}

}  // namespace ccamera
