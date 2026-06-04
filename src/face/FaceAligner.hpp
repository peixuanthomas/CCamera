#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>

namespace ccamera {

class FaceAligner {
public:
    explicit FaceAligner(cv::Size output_size = {112, 112});
    cv::Mat align(const cv::Mat& frame, const FaceDetection& detection) const;

private:
    cv::Size output_size_;
};

}  // namespace ccamera
