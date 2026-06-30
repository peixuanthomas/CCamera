#pragma once

#include "face/FaceServices.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>

#include <string>

namespace ccamera {

class InsightFaceArcFaceFeatureExtractor final : public FaceEmbeddingService {
public:
    explicit InsightFaceArcFaceFeatureExtractor(
        InferenceEngine& engine,
        std::string input_name = "data",
        std::string output_name = "fc1");

    FaceEmbedding extract(const cv::Mat& aligned_face) override;
    FaceEmbedding embed(const cv::Mat& frame, const FaceDetection& detection, const FaceAligner& aligner) override;

private:
    InferenceEngine& engine_;
    std::string input_name_;
    std::string output_name_;
};

}  // namespace ccamera
