#include "face/FaceAligner.hpp"

#include <opencv2/imgproc.hpp>

#include <stdexcept>

namespace ccamera {

FaceAligner::FaceAligner(cv::Size output_size) : output_size_(output_size) {
    if (output_size_.width <= 0 || output_size_.height <= 0) {
        throw std::invalid_argument("face aligner output size must be positive");
    }
}

cv::Mat FaceAligner::align(const cv::Mat& frame, const FaceDetection& detection) const {
    if (frame.empty()) {
        throw std::runtime_error("cannot align face from an empty frame");
    }

    const cv::Rect image_bounds(0, 0, frame.cols, frame.rows);
    cv::Rect crop = detection.box;
    const int margin_x = static_cast<int>(crop.width * 0.15f);
    const int margin_y = static_cast<int>(crop.height * 0.15f);
    crop.x -= margin_x;
    crop.y -= margin_y;
    crop.width += margin_x * 2;
    crop.height += margin_y * 2;
    crop &= image_bounds;

    if (crop.width < 20 || crop.height < 20) {
        throw std::runtime_error("face crop is too small");
    }

    cv::Mat resized;
    cv::resize(frame(crop), resized, output_size_);
    return resized;
}

}  // namespace ccamera
