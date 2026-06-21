#include "web/MjpegStream.hpp"

#include <cassert>
#include <opencv2/core.hpp>

#include <string>

int main() {
    cv::Mat image(80, 120, CV_8UC3, cv::Scalar(10, 20, 30));

    ccamera::RecognitionResult result;
    result.label = "Alice";
    result.known = true;
    result.similarity = 0.93f;
    result.detection.box = cv::Rect2f(10.0f, 10.0f, 40.0f, 30.0f);

    const auto jpeg = ccamera::encode_overlay_jpeg(image, {result}, "Ready");
    assert(jpeg.size() > 10);
    assert(jpeg[0] == 0xff);
    assert(jpeg[1] == 0xd8);

    const auto part = ccamera::format_mjpeg_part(jpeg);
    const std::string part_text(part.begin(), part.end());
    assert(part_text.find("--ccamera\r\n") == 0);
    assert(part_text.find("Content-Type: image/jpeg\r\n") != std::string::npos);
    assert(part_text.find("Content-Length: " + std::to_string(jpeg.size())) != std::string::npos);

    const auto fallback = ccamera::make_web_fallback_frame({320, 180}, "Waiting for camera");
    assert(fallback.cols == 320);
    assert(fallback.rows == 180);
    assert(fallback.type() == CV_8UC3);

    return 0;
}
