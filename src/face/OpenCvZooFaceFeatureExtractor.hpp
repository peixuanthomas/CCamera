#pragma once

#include "face/FaceServices.hpp"

#include <opencv2/core.hpp>
#include <opencv2/objdetect/face.hpp>

#include <string>

namespace ccamera {

FaceEmbedding face_embedding_from_sface_feature(const cv::Mat& feature);

class OpenCvZooFaceFeatureExtractor final : public FaceEmbeddingService {
public:
    explicit OpenCvZooFaceFeatureExtractor(const std::string& model_path);
    FaceEmbedding extract(const cv::Mat& aligned_face) override;

private:
    cv::Ptr<cv::FaceRecognizerSF> recognizer_;
};

}  // namespace ccamera
