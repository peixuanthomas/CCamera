#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>

#include <vector>

namespace ccamera {

class FaceAligner;

class FaceDetectionService {
public:
    virtual ~FaceDetectionService() = default;
    virtual std::vector<FaceDetection> detect(const cv::Mat& frame) = 0;
};

class FaceEmbeddingService {
public:
    virtual ~FaceEmbeddingService() = default;
    virtual FaceEmbedding extract(const cv::Mat& aligned_face) = 0;

    // Align the detected face from the full frame and extract its embedding.
    // The default uses the supplied bounding-box aligner. Backends that have a
    // landmark-based alignment (e.g. OpenCV Zoo SFace alignCrop) override this so
    // the recognizer receives the geometrically aligned crop it expects.
    virtual FaceEmbedding embed(const cv::Mat& frame, const FaceDetection& detection, const FaceAligner& aligner);
};

}  // namespace ccamera
