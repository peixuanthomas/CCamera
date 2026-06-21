#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace ccamera {

cv::Mat make_web_fallback_frame(cv::Size size, const std::string& status);
std::vector<unsigned char> encode_overlay_jpeg(
    const cv::Mat& frame,
    const std::vector<RecognitionResult>& results,
    const std::string& status);
std::vector<unsigned char> format_mjpeg_part(const std::vector<unsigned char>& jpeg);

}  // namespace ccamera
