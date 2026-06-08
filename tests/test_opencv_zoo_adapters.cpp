#include "core/Math.hpp"
#include "face/OpenCvZooFaceDetector.hpp"
#include "face/OpenCvZooFaceFeatureExtractor.hpp"

#include <cassert>
#include <cmath>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>

namespace {

template <typename Callable>
void expect_runtime_error(Callable&& callable, const std::string& expected) {
    try {
        callable();
    } catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(expected) != std::string::npos);
        return;
    }
    assert(false);
}

bool close(float a, float b) {
    return std::abs(a - b) < 1.0e-5f;
}

}  // namespace

int main() {
    cv::Mat row(1, 15, CV_32F);
    float* values = row.ptr<float>(0);
    values[0] = 10.0f;   // x
    values[1] = 20.0f;   // y
    values[2] = 100.0f;  // width
    values[3] = 80.0f;   // height
    values[4] = 80.0f;   // right eye x
    values[5] = 45.0f;   // right eye y
    values[6] = 35.0f;   // left eye x
    values[7] = 45.0f;   // left eye y
    values[8] = 60.0f;   // nose x
    values[9] = 65.0f;   // nose y
    values[10] = 78.0f;  // right mouth x
    values[11] = 92.0f;  // right mouth y
    values[12] = 42.0f;  // left mouth x
    values[13] = 92.0f;  // left mouth y
    values[14] = 0.91f;  // score

    const auto face = ccamera::face_detection_from_yunet_row(row, 0);
    assert(close(face.box.x, 10.0f));
    assert(close(face.box.y, 20.0f));
    assert(close(face.box.width, 100.0f));
    assert(close(face.box.height, 80.0f));
    assert(close(face.score, 0.91f));
    assert(face.keypoints.left_eye == cv::Point2f(35.0f, 45.0f));
    assert(face.keypoints.right_eye == cv::Point2f(80.0f, 45.0f));
    assert(face.keypoints.nose == cv::Point2f(60.0f, 65.0f));
    assert(face.keypoints.mouth_left == cv::Point2f(42.0f, 92.0f));
    assert(face.keypoints.mouth_right == cv::Point2f(78.0f, 92.0f));

    cv::Mat bad(1, 14, CV_32F);
    expect_runtime_error(
        [&] { (void)ccamera::face_detection_from_yunet_row(bad, 0); },
        "YuNet detection output");

    cv::Mat feature = (cv::Mat_<float>(1, 4) << 3.0f, 0.0f, 4.0f, 0.0f);
    const auto embedding = ccamera::face_embedding_from_sface_feature(feature);
    assert(embedding.values.size() == 4);
    assert(close(ccamera::l2_norm(embedding.values), 1.0f));
    assert(close(embedding.values[0], 0.6f));
    assert(close(embedding.values[2], 0.8f));

    cv::Mat bad_feature(1, 0, CV_32F);
    expect_runtime_error(
        [&] { (void)ccamera::face_embedding_from_sface_feature(bad_feature); },
        "SFace feature");

    return 0;
}
