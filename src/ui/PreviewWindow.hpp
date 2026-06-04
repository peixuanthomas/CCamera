#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace ccamera {

class PreviewWindow {
public:
    explicit PreviewWindow(std::string name);
    void show(const cv::Mat& frame, const std::vector<RecognitionResult>& results, double fps);
    static void draw_overlay(cv::Mat& frame, const std::vector<RecognitionResult>& results);

private:
    std::string name_;
};

}  // namespace ccamera
