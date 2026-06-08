#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>

#include <vector>

namespace ccamera {

class FaceDetectionService {
public:
    virtual ~FaceDetectionService() = default;
    virtual std::vector<FaceDetection> detect(const cv::Mat& frame) = 0;
};

class FaceEmbeddingService {
public:
    virtual ~FaceEmbeddingService() = default;
    virtual FaceEmbedding extract(const cv::Mat& aligned_face) = 0;
};

}  // namespace ccamera
