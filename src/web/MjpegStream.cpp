#include "web/MjpegStream.hpp"

#include "ui/PreviewWindow.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <stdexcept>
#include <string>

namespace ccamera {

cv::Mat make_web_fallback_frame(cv::Size size, const std::string& status) {
    const int width = size.width > 0 ? size.width : 640;
    const int height = size.height > 0 ? size.height : 360;
    cv::Mat frame(height, width, CV_8UC3, cv::Scalar(24, 24, 24));
    const std::string text = status.empty() ? "Waiting for camera" : status;
    cv::putText(frame, text, {24, height / 2}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(230, 230, 230), 2, cv::LINE_AA);
    return frame;
}

std::vector<unsigned char> encode_overlay_jpeg(
    const cv::Mat& frame,
    const std::vector<RecognitionResult>& results,
    const std::string& status) {
    cv::Mat output = frame.empty() ? make_web_fallback_frame({640, 360}, status) : frame.clone();
    PreviewWindow::draw_overlay(output, results);
    if (!status.empty()) {
        cv::putText(output, status, {16, 32}, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 215, 255), 2, cv::LINE_AA);
    }

    std::vector<unsigned char> jpeg;
    if (!cv::imencode(".jpg", output, jpeg, {cv::IMWRITE_JPEG_QUALITY, 82})) {
        throw std::runtime_error("failed to encode MJPEG frame");
    }
    return jpeg;
}

std::vector<unsigned char> format_mjpeg_part(const std::vector<unsigned char>& jpeg) {
    const std::string header =
        "--ccamera\r\nContent-Type: image/jpeg\r\nContent-Length: " +
        std::to_string(jpeg.size()) + "\r\n\r\n";
    std::vector<unsigned char> part(header.begin(), header.end());
    part.insert(part.end(), jpeg.begin(), jpeg.end());
    const std::string trailer = "\r\n";
    part.insert(part.end(), trailer.begin(), trailer.end());
    return part;
}

}  // namespace ccamera
