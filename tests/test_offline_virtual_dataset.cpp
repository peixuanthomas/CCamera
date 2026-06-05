#include "support/VirtualFaceDataset.hpp"

#include "face/FaceAligner.hpp"
#include "face/FaceDetector.hpp"
#include "face/FaceFeatureExtractor.hpp"
#include "face/Recognizer.hpp"

#include <cassert>

int main() {
    const ccamera::test_support::VirtualFaceDataset dataset =
        ccamera::test_support::make_single_face_virtual_dataset();

    ccamera::test_support::VirtualDatasetDetectorEngine detector_engine(dataset);
    ccamera::test_support::VirtualDatasetRecognizerEngine recognizer_engine(dataset);
    ccamera::FaceDetector detector(detector_engine, 0.7f);
    ccamera::FaceAligner aligner;
    ccamera::FaceFeatureExtractor extractor(recognizer_engine);
    ccamera::Recognizer recognizer(0.8f);

    const ccamera::FaceDatabaseSnapshot database =
        ccamera::test_support::make_virtual_database_snapshot(dataset);
    const std::vector<ccamera::RecognitionResult> results =
        ccamera::test_support::run_virtual_recognition_frame(
            dataset.frames.front(),
            detector,
            aligner,
            extractor,
            database,
            recognizer);

    assert(results.size() == 1);
    assert(results.front().frame_sequence == dataset.frames.front().sequence);
    assert(results.front().known);
    assert(results.front().label == dataset.identity_name);
    assert(results.front().similarity > 0.99f);
    assert(detector_engine.run_count() == 1);
    assert(recognizer_engine.run_count() == 1);

    return 0;
}
