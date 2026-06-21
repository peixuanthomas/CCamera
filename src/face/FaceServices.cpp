#include "face/FaceServices.hpp"

#include "face/FaceAligner.hpp"

namespace ccamera {

FaceEmbedding FaceEmbeddingService::embed(
    const cv::Mat& frame,
    const FaceDetection& detection,
    const FaceAligner& aligner) {
    return extract(aligner.align(frame, detection));
}

}  // namespace ccamera
