#pragma once

#include "core/Types.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>
#include <vector>

namespace ccamera {

class FaceDetector {
public:
    FaceDetector(InferenceEngine& engine, float threshold);
    std::vector<FaceDetection> detect(const cv::Mat& frame);

private:
    InferenceEngine& engine_;
    float threshold_;
};

}  // namespace ccamera
