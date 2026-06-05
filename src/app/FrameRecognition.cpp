#include "app/FrameRecognition.hpp"

#include <exception>
#include <utility>

namespace ccamera {

std::vector<RecognitionResult> recognize_frame_faces(
    const Frame& frame,
    const std::vector<FaceDetection>& faces,
    const FaceAligner& aligner,
    FaceFeatureExtractor& extractor,
    const FaceDatabaseSnapshot& database,
    const Recognizer& recognizer) {
    std::vector<RecognitionResult> results;
    for (const auto& face : faces) {
        RecognitionResult result;
        result.detection = face;
        result.frame_sequence = frame.sequence;
        try {
            const cv::Mat aligned = aligner.align(frame.image, face);
            const FaceEmbedding embedding = extractor.extract(aligned);
            const MatchResult match = recognizer.match(embedding, database);
            result.label = match.label;
            result.similarity = match.similarity;
            result.known = match.known;
        } catch (const std::exception&) {
            result.label = "Invalid face";
            result.similarity = 0.0f;
            result.known = false;
        }
        results.push_back(std::move(result));
    }
    return results;
}

}  // namespace ccamera
