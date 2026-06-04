#include "ui/PreviewWindow.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <sstream>
#include <utility>

namespace ccamera {

PreviewWindow::PreviewWindow(std::string name) : name_(std::move(name)) {
    cv::namedWindow(name_, cv::WINDOW_NORMAL);
}

void PreviewWindow::show(const cv::Mat& frame, const std::vector<RecognitionResult>& results, double fps) {
    cv::Mat output = frame.clone();
    draw_overlay(output, results);

    std::ostringstream text;
    text << "FPS: " << static_cast<int>(fps);
    cv::putText(output, text.str(), {12, 28}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);

    cv::imshow(name_, output);
}

}  // namespace ccamera
