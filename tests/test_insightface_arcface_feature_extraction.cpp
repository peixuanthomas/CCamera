#include "core/Math.hpp"
#include "face/FaceAligner.hpp"
#include "face/InsightFaceArcFaceFeatureExtractor.hpp"

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
    int run_count = 0;

    ccamera::TensorMap run(const ccamera::TensorMap& inputs) override {
        ++run_count;
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

std::vector<float> embedding_with(float first, float second) {
    std::vector<float> values(512, 0.0f);
    values[0] = first;
    values[1] = second;
    return values;
}

cv::Mat make_112_bgr_face() {
    cv::Mat face(112, 112, CV_8UC3, cv::Scalar(0, 0, 0));
    face.at<cv::Vec3b>(0, 0) = {0, 127, 255};
    face.at<cv::Vec3b>(0, 1) = {255, 127, 0};
    return face;
}

ccamera::FaceDetection detection_with_arcface_template_landmarks() {
    ccamera::FaceDetection detection;
    detection.box = cv::Rect2f(1.0f, 1.0f, 1.0f, 1.0f);
    detection.score = 0.99f;
    detection.keypoints.left_eye = cv::Point2f{38.2946f, 51.6963f};
    detection.keypoints.right_eye = cv::Point2f{73.5318f, 51.5014f};
    detection.keypoints.nose = cv::Point2f{56.0252f, 71.7366f};
    detection.keypoints.mouth_left = cv::Point2f{41.5493f, 92.3655f};
    detection.keypoints.mouth_right = cv::Point2f{70.7299f, 92.2041f};
    return detection;
}

void test_extract_packs_rgb_normalized_nchw_and_uses_configured_names() {
    FakeInferenceEngine engine;
    engine.outputs["custom_output"] = {{512}, embedding_with(3.0f, 4.0f)};

    ccamera::InsightFaceArcFaceFeatureExtractor extractor(engine, "custom_input", "custom_output");
    const ccamera::FaceEmbedding embedding = extractor.extract(make_112_bgr_face());

    const auto found = engine.last_inputs.find("custom_input");
    assert(found != engine.last_inputs.end());
    assert(engine.last_inputs.find("data") == engine.last_inputs.end());
    assert((found->second.shape == std::vector<int64_t>{1, 3, 112, 112}));
    assert(found->second.values.size() == 3 * 112 * 112);

    const std::size_t plane_size = 112 * 112;
    assert(close_to(found->second.values[0], 1.0f));
    assert(close_to(found->second.values[1], -1.0f));
    assert(close_to(found->second.values[plane_size], (127.0f - 127.5f) / 127.5f));
    assert(close_to(found->second.values[plane_size + 1], (127.0f - 127.5f) / 127.5f));
    assert(close_to(found->second.values[plane_size * 2], -1.0f));
    assert(close_to(found->second.values[plane_size * 2 + 1], 1.0f));

    assert(embedding.values.size() == 512);
    assert(close_to(embedding.values[0], 0.6f));
    assert(close_to(embedding.values[1], 0.8f));
    assert(close_to(ccamera::l2_norm(embedding.values), 1.0f));
}

void test_extract_accepts_single_batch_2d_output() {
    FakeInferenceEngine engine;
    std::vector<float> values(512, 0.0f);
    values[7] = 5.0f;
    engine.outputs["fc1"] = {{1, 512}, values};

    ccamera::InsightFaceArcFaceFeatureExtractor extractor(engine);
    const ccamera::FaceEmbedding embedding = extractor.extract(make_112_bgr_face());

    assert(embedding.values.size() == 512);
    assert(close_to(embedding.values[7], 1.0f));
    assert(close_to(ccamera::l2_norm(embedding.values), 1.0f));
}

void test_extract_rejects_non_512_outputs() {
    FakeInferenceEngine engine;
    engine.outputs["fc1"] = {{511}, std::vector<float>(511, 1.0f)};

    ccamera::InsightFaceArcFaceFeatureExtractor extractor(engine);
    expect_throws<std::runtime_error>(
        [&] { (void)extractor.extract(make_112_bgr_face()); },
        "[512]");
}

void test_embed_uses_landmarks_without_bbox_fallback() {
    FakeInferenceEngine engine;
    engine.outputs["fc1"] = {{512}, embedding_with(0.0f, 2.0f)};

    ccamera::InsightFaceArcFaceFeatureExtractor extractor(engine);
    ccamera::FaceAligner aligner;
    const ccamera::FaceEmbedding embedding =
        extractor.embed(make_112_bgr_face(), detection_with_arcface_template_landmarks(), aligner);

    assert(engine.run_count == 1);
    assert(embedding.values.size() == 512);
    assert(close_to(embedding.values[1], 1.0f));
}

void test_embed_rejects_missing_keypoints() {
    FakeInferenceEngine engine;
    engine.outputs["fc1"] = {{512}, embedding_with(1.0f, 0.0f)};

    ccamera::InsightFaceArcFaceFeatureExtractor extractor(engine);
    ccamera::FaceAligner aligner;
    ccamera::FaceDetection detection = detection_with_arcface_template_landmarks();
    detection.keypoints.nose.reset();

    expect_throws<std::runtime_error>(
        [&] { (void)extractor.embed(make_112_bgr_face(), detection, aligner); },
        "requires five landmarks");
    assert(engine.run_count == 0);
}

}  // namespace

int main() {
    test_extract_packs_rgb_normalized_nchw_and_uses_configured_names();
    test_extract_accepts_single_batch_2d_output();
    test_extract_rejects_non_512_outputs();
    test_embed_uses_landmarks_without_bbox_fallback();
    test_embed_rejects_missing_keypoints();
    return 0;
}
