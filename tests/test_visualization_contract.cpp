#include "face/FaceDetector.hpp"
#include "inference/InferenceEngine.hpp"
#include "ui/PreviewWindow.hpp"

#include <cassert>
#include <opencv2/core.hpp>

namespace {

class FakeDetectorEngine final : public ccamera::InferenceEngine {
public:
    ccamera::TensorMap run(const ccamera::TensorMap& inputs) override {
        const auto image = inputs.find("image");
        assert(image != inputs.end());
        assert((image->second.shape == std::vector<int64_t>{1, 3, 160, 160}));
        assert(image->second.values.size() == 3 * 160 * 160);
        assert(image->second.values[7 * 160 + 9] == 11.0f);
        assert(image->second.values[160 * 160 + 7 * 160 + 9] == 22.0f);
        assert(image->second.values[2 * 160 * 160 + 7 * 160 + 9] == 33.0f);

        ccamera::Tensor detections;
        detections.shape = {1, 15};
        detections.values = {
            10.0f, 20.0f, 110.0f, 120.0f, 0.91f,
            35.0f, 50.0f,
            85.0f, 50.0f,
            60.0f, 75.0f,
            45.0f, 100.0f,
            75.0f, 100.0f
        };
        return {{"detections", detections}};
    }
};

void assert_red_pixel(const cv::Mat& image, int x, int y) {
    const cv::Vec3b pixel = image.at<cv::Vec3b>(y, x);
    assert(pixel[2] > 180);
    assert(pixel[1] < 80);
    assert(pixel[0] < 80);
}

}  // namespace

int main() {
    FakeDetectorEngine engine;
    ccamera::FaceDetector detector(engine, 0.7f);
    cv::Mat frame = cv::Mat::zeros(160, 160, CV_8UC3);
    frame.at<cv::Vec3b>(7, 9) = cv::Vec3b(11, 22, 33);

    const auto faces = detector.detect(frame);
    assert(faces.size() == 1);
    assert(faces[0].score > 0.9f);
    assert(faces[0].keypoints.left_eye.has_value());
    assert(faces[0].keypoints.right_eye.has_value());
    assert(faces[0].keypoints.nose.has_value());
    assert(faces[0].keypoints.mouth_left.has_value());
    assert(faces[0].keypoints.mouth_right.has_value());
    assert(*faces[0].keypoints.left_eye == cv::Point2f(35.0f, 50.0f));
    assert(*faces[0].keypoints.right_eye == cv::Point2f(85.0f, 50.0f));
    assert(*faces[0].keypoints.nose == cv::Point2f(60.0f, 75.0f));
    assert(*faces[0].keypoints.mouth_left == cv::Point2f(45.0f, 100.0f));
    assert(*faces[0].keypoints.mouth_right == cv::Point2f(75.0f, 100.0f));

    cv::Mat overlay = frame.clone();
    ccamera::PreviewWindow::draw_overlay(overlay, {{faces[0], "Alice", 0.93f, true}});

    assert_red_pixel(overlay, 35, 50);
    assert_red_pixel(overlay, 85, 50);
    assert_red_pixel(overlay, 60, 75);
    assert_red_pixel(overlay, 45, 100);
    assert_red_pixel(overlay, 75, 100);

    return 0;
}
