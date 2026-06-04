#include "face/FaceAligner.hpp"
#include "face/FaceFeatureExtractor.hpp"

#include <opencv2/core.hpp>

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class FakeInferenceEngine final : public ccamera::InferenceEngine {
public:
    ccamera::TensorMap outputs;
    ccamera::TensorMap last_inputs;

    ccamera::TensorMap run(const ccamera::TensorMap& inputs) override {
        last_inputs = inputs;
        return outputs;
    }
};

bool close_to(float actual, float expected) {
    return std::fabs(actual - expected) < 1.0e-5f;
}

template <typename Exception, typename Callable>
void expect_throws(Callable&& callable, const std::string& expected_message) {
    try {
        callable();
    } catch (const Exception& error) {
        assert(std::string(error.what()).find(expected_message) != std::string::npos);
        return;
    }
    assert(false);
}

void test_aligner_expands_crop_to_bounds_and_resizes() {
    cv::Mat frame(100, 100, CV_8UC1);
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            frame.at<unsigned char>(y, x) = static_cast<unsigned char>(y + x);
        }
    }

    ccamera::FaceDetection detection;
    detection.box = cv::Rect2f(0.0f, 0.0f, 40.0f, 40.0f);

    ccamera::FaceAligner aligner({46, 46});
    const cv::Mat aligned = aligner.align(frame, detection);

    assert(aligned.rows == 46);
    assert(aligned.cols == 46);
    assert(aligned.type() == CV_8UC1);
    assert(aligned.at<unsigned char>(0, 0) == frame.at<unsigned char>(0, 0));
    assert(aligned.at<unsigned char>(45, 45) == frame.at<unsigned char>(45, 45));
}

void test_aligner_rejects_invalid_inputs() {
    expect_throws<std::invalid_argument>(
        [] { ccamera::FaceAligner aligner({0, 112}); },
        "output size");

    ccamera::FaceAligner aligner;
    ccamera::FaceDetection detection;
    detection.box = cv::Rect2f(20.0f, 20.0f, 10.0f, 10.0f);

    expect_throws<std::runtime_error>(
        [&] { aligner.align(cv::Mat{}, detection); },
        "empty frame");

    const cv::Mat frame = cv::Mat::zeros(100, 100, CV_8UC3);
    expect_throws<std::runtime_error>(
        [&] { aligner.align(frame, detection); },
        "face crop is too small");
}

void test_feature_extractor_packs_input_and_normalizes_1d_embedding() {
    FakeInferenceEngine engine;
    engine.outputs["embedding"] = {{3}, {3.0f, 4.0f, 0.0f}};

    cv::Mat aligned_face(2, 2, CV_8UC3);
    aligned_face.at<cv::Vec3b>(0, 0) = {1, 2, 3};
    aligned_face.at<cv::Vec3b>(0, 1) = {4, 5, 6};
    aligned_face.at<cv::Vec3b>(1, 0) = {7, 8, 9};
    aligned_face.at<cv::Vec3b>(1, 1) = {10, 11, 12};

    ccamera::FaceFeatureExtractor extractor(engine);
    const ccamera::FaceEmbedding embedding = extractor.extract(aligned_face);

    const auto found = engine.last_inputs.find("face");
    assert(found != engine.last_inputs.end());
    assert((found->second.shape == std::vector<int64_t>{1, 3, 2, 2}));
    assert((found->second.values == std::vector<float>{1.0f, 4.0f, 7.0f, 10.0f,
                                                        2.0f, 5.0f, 8.0f, 11.0f,
                                                        3.0f, 6.0f, 9.0f, 12.0f}));

    assert(embedding.values.size() == 3);
    assert(close_to(embedding.values[0], 0.6f));
    assert(close_to(embedding.values[1], 0.8f));
    assert(close_to(embedding.values[2], 0.0f));
}

void test_feature_extractor_accepts_single_batch_2d_embedding() {
    FakeInferenceEngine engine;
    engine.outputs["embedding"] = {{1, 2}, {5.0f, 0.0f}};

    const cv::Mat aligned_face = cv::Mat::zeros(1, 1, CV_8UC1);
    ccamera::FaceFeatureExtractor extractor(engine);
    const ccamera::FaceEmbedding embedding = extractor.extract(aligned_face);

    assert(embedding.values.size() == 2);
    assert(close_to(embedding.values[0], 1.0f));
    assert(close_to(embedding.values[1], 0.0f));
}

void test_feature_extractor_rejects_malformed_outputs() {
    const cv::Mat aligned_face = cv::Mat::zeros(1, 1, CV_8UC1);

    {
        FakeInferenceEngine engine;
        ccamera::FaceFeatureExtractor extractor(engine);
        expect_throws<std::runtime_error>(
            [&] { extractor.extract(aligned_face); },
            "embedding");
    }

    {
        FakeInferenceEngine engine;
        engine.outputs["embedding"] = {{0}, {}};
        ccamera::FaceFeatureExtractor extractor(engine);
        expect_throws<std::runtime_error>(
            [&] { extractor.extract(aligned_face); },
            "dimension");
    }

    {
        FakeInferenceEngine engine;
        engine.outputs["embedding"] = {{2, 3}, {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f}};
        ccamera::FaceFeatureExtractor extractor(engine);
        expect_throws<std::runtime_error>(
            [&] { extractor.extract(aligned_face); },
            "batch size");
    }

    {
        FakeInferenceEngine engine;
        engine.outputs["embedding"] = {{1, 3}, {1.0f, 0.0f}};
        ccamera::FaceFeatureExtractor extractor(engine);
        expect_throws<std::runtime_error>(
            [&] { extractor.extract(aligned_face); },
            "values");
    }
}

void test_average_embeddings_rejects_dimension_mismatch() {
    expect_throws<std::invalid_argument>(
        [] { ccamera::average_embeddings({{{1.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}}); },
        "dimension mismatch");
}

}  // namespace

int main() {
    test_aligner_expands_crop_to_bounds_and_resizes();
    test_aligner_rejects_invalid_inputs();
    test_feature_extractor_packs_input_and_normalizes_1d_embedding();
    test_feature_extractor_accepts_single_batch_2d_embedding();
    test_feature_extractor_rejects_malformed_outputs();
    test_average_embeddings_rejects_dimension_mismatch();
    return 0;
}
