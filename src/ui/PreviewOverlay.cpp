#include "ui/PreviewWindow.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <optional>
#include <sstream>

namespace ccamera {
namespace {

void draw_keypoint(cv::Mat& frame, const std::optional<cv::Point2f>& point) {
    if (!point.has_value()) {
        return;
    }
    cv::circle(frame, *point, 3, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA);
}

}  // namespace

void PreviewWindow::draw_overlay(cv::Mat& frame, const std::vector<RecognitionResult>& results) {
    for (const auto& result : results) {
        const auto color = result.known ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 255, 255);
        cv::rectangle(frame, result.detection.box, color, 2, cv::LINE_AA);

        const auto& points = result.detection.keypoints;
        draw_keypoint(frame, points.left_eye);
        draw_keypoint(frame, points.right_eye);
        draw_keypoint(frame, points.nose);
        draw_keypoint(frame, points.mouth_left);
        draw_keypoint(frame, points.mouth_right);

        std::ostringstream label;
        label << result.label << " " << static_cast<int>(result.similarity * 100.0f) << "%";
        const cv::Point origin(
            static_cast<int>(result.detection.box.x),
            std::max(20, static_cast<int>(result.detection.box.y) - 6));
        cv::putText(frame, label.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2, cv::LINE_AA);
    }
}

}  // namespace ccamera
