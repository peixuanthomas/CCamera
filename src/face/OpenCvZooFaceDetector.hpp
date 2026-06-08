#pragma once

#include "face/FaceServices.hpp"

#include <opencv2/core.hpp>
#include <opencv2/objdetect/face.hpp>

#include <string>
#include <vector>

namespace ccamera {

FaceDetection face_detection_from_yunet_row(const cv::Mat& detections, int row_index);

class OpenCvZooFaceDetector final : public FaceDetectionService {
public:
    OpenCvZooFaceDetector(const std::string& model_path, float threshold);
    std::vector<FaceDetection> detect(const cv::Mat& frame) override;

private:
    cv::Ptr<cv::FaceDetectorYN> detector_;
    cv::Size input_size_;
};

}  // namespace ccamera
