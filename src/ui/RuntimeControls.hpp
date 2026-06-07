#pragma once

#include "app/CommandLoop.hpp"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace ccamera {

enum class RuntimeControlMode {
    Idle,
    EnteringEnrollName,
    EnteringDeleteName,
};

struct RuntimeControlsLayout {
    cv::Rect panel;
    cv::Rect enroll_button;
    cv::Rect delete_button;
    cv::Rect list_button;
    cv::Rect quit_button;
};

class RuntimeControls {
public:
    RuntimeControlsLayout layout_for(cv::Size frame_size) const;
    std::vector<Command> click(int x, int y, const RuntimeControlsLayout& layout);
    std::vector<Command> key(int key);

    RuntimeControlMode mode() const;
    const std::string& input_text() const;
    std::string prompt_text() const;

private:
    RuntimeControlMode mode_ = RuntimeControlMode::Idle;
    std::string input_text_;
};

}  // namespace ccamera
