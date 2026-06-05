#include "app/FrameRecognition.hpp"
#include "app/RecognitionResults.hpp"
#include "support/VirtualFaceDataset.hpp"

#include "face/FaceAligner.hpp"
#include "face/FaceDetector.hpp"
#include "face/FaceFeatureExtractor.hpp"
#include "face/Recognizer.hpp"

#include <cassert>
#include <cmath>

namespace {

bool close_to(float actual, float expected) {
    return std::fabs(actual - expected) < 1.0e-5f;
}

void assert_point(const std::optional<cv::Point2f>& actual, const cv::Point2f& expected) {
    assert(actual.has_value());
    assert(close_to(actual->x, expected.x));
    assert(close_to(actual->y, expected.y));
}

void assert_same_embedding(const std::vector<float>& actual, const std::vector<float>& expected) {
    assert(actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) {
        assert(close_to(actual[i], expected[i]));
    }
}

}  // namespace

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

    const std::vector<ccamera::FaceDetection> faces = detector.detect(dataset.frames.front().image);
    assert(faces.size() == 1);
    assert(faces.front().box == dataset.expected_box);
    assert_point(faces.front().keypoints.left_eye, *dataset.expected_keypoints.left_eye);
    assert_point(faces.front().keypoints.right_eye, *dataset.expected_keypoints.right_eye);
    assert_point(faces.front().keypoints.nose, *dataset.expected_keypoints.nose);
    assert_point(faces.front().keypoints.mouth_left, *dataset.expected_keypoints.mouth_left);
    assert_point(faces.front().keypoints.mouth_right, *dataset.expected_keypoints.mouth_right);

    ccamera::RecognitionResultsStore store;
    const bool ok = ccamera::run_recognition_step(store, dataset.frames.front().sequence, [&] {
        return ccamera::recognize_frame_faces(
            dataset.frames.front(),
            faces,
            aligner,
            extractor,
            database,
            recognizer);
    });
    assert(ok);

    const std::vector<ccamera::RecognitionResult> results =
        store.results_for_sequence(dataset.frames.front().sequence);

    assert(results.size() == 1);
    assert(results.front().frame_sequence == dataset.frames.front().sequence);
    assert(results.front().detection.box == dataset.expected_box);
    assert(results.front().known);
    assert(results.front().label == dataset.identity_name);
    assert(results.front().similarity > 0.99f);
    assert(detector_engine.run_count() == 1);
    assert(recognizer_engine.run_count() == 1);
    assert_same_embedding(recognizer_engine.last_raw_embedding(), dataset.expected_recognizer_output.values);

    ccamera::FaceDetection too_small = faces.front();
    too_small.box = cv::Rect2f(1.0f, 1.0f, 8.0f, 8.0f);
    const std::vector<ccamera::RecognitionResult> invalid_results =
        ccamera::recognize_frame_faces(
            dataset.frames.front(),
            {too_small},
            aligner,
            extractor,
            database,
            recognizer);
    assert(invalid_results.size() == 1);
    assert(!invalid_results.front().known);
    assert(invalid_results.front().label == "Invalid face");
    assert(invalid_results.front().frame_sequence == dataset.frames.front().sequence);

    return 0;
}
