#pragma once

#include "face/FaceServices.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>

#include <vector>

namespace ccamera {

class FaceFeatureExtractor final : public FaceEmbeddingService {
public:
    explicit FaceFeatureExtractor(InferenceEngine& engine);
    FaceEmbedding extract(const cv::Mat& aligned_face) override;

private:
    InferenceEngine& engine_;
};

FaceEmbedding average_embeddings(const std::vector<FaceEmbedding>& embeddings);

}  // namespace ccamera
