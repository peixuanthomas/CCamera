#pragma once

#include "core/Types.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>

#include <vector>

namespace ccamera {

class FaceFeatureExtractor {
public:
    explicit FaceFeatureExtractor(InferenceEngine& engine);
    FaceEmbedding extract(const cv::Mat& aligned_face);

private:
    InferenceEngine& engine_;
};

FaceEmbedding average_embeddings(const std::vector<FaceEmbedding>& embeddings);

}  // namespace ccamera
