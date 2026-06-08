#pragma once

#include "face/FaceServices.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>
#include <vector>

namespace ccamera {

class FaceDetector final : public FaceDetectionService {
public:
    FaceDetector(InferenceEngine& engine, float threshold);
    std::vector<FaceDetection> detect(const cv::Mat& frame) override;

private:
    InferenceEngine& engine_;
    float threshold_;
};

}  // namespace ccamera
