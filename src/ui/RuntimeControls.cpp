#include "ui/RuntimeControls.hpp"

#include <algorithm>

namespace ccamera {
namespace {

bool contains(const cv::Rect& rect, int x, int y) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height;
}

bool is_enter_key(int key) {
    return key == '\r' || key == '\n' || key == 13;
}

bool is_backspace_key(int key) {
    return key == '\b' || key == 8 || key == 127;
}

bool is_escape_key(int key) {
    return key == 27;
}

bool is_supported_name_char(int key) {
    return key >= 32 && key <= 126;
}

std::vector<Command> command_if_named(CommandType type, const std::string& name) {
    if (name.empty()) {
        return {};
    }
    return {Command{type, name}};
}

}  // namespace

RuntimeControlsLayout RuntimeControls::layout_for(cv::Size frame_size) const {
    const int panel_width = std::min(280, std::max(180, frame_size.width / 3));
    const int x = std::max(0, frame_size.width - panel_width);
    const int button_width = std::max(120, panel_width - 32);
    const int button_height = 38;
    const int left = x + 16;
    const int top = 58;
    const int gap = 12;

    RuntimeControlsLayout layout;
    layout.panel = cv::Rect(x, 0, panel_width, std::max(1, frame_size.height));
    layout.enroll_button = cv::Rect(left, top, button_width, button_height);
    layout.delete_button = cv::Rect(left, top + (button_height + gap), button_width, button_height);
    layout.list_button = cv::Rect(left, top + 2 * (button_height + gap), button_width, button_height);
    layout.quit_button = cv::Rect(left, top + 3 * (button_height + gap), button_width, button_height);
    return layout;
}

std::vector<Command> RuntimeControls::click(int x, int y, const RuntimeControlsLayout& layout) {
    if (contains(layout.enroll_button, x, y)) {
        mode_ = RuntimeControlMode::EnteringEnrollName;
        input_text_.clear();
        return {};
    }
    if (contains(layout.delete_button, x, y)) {
        mode_ = RuntimeControlMode::EnteringDeleteName;
        input_text_.clear();
        return {};
    }
    if (contains(layout.list_button, x, y)) {
        return {Command{CommandType::List, {}}};
    }
    if (contains(layout.quit_button, x, y)) {
        return {Command{CommandType::Quit, {}}};
    }
    return {};
}

std::vector<Command> RuntimeControls::key(int key) {
    if (mode_ == RuntimeControlMode::Idle) {
        return {};
    }
    if (is_escape_key(key)) {
        mode_ = RuntimeControlMode::Idle;
        input_text_.clear();
        return {};
    }
    if (is_backspace_key(key)) {
        if (!input_text_.empty()) {
            input_text_.pop_back();
        }
        return {};
    }
    if (is_enter_key(key)) {
        const auto type = mode_ == RuntimeControlMode::EnteringEnrollName ? CommandType::Enroll : CommandType::Delete;
        const std::string name = input_text_;
        mode_ = RuntimeControlMode::Idle;
        input_text_.clear();
        return command_if_named(type, name);
    }
    if (is_supported_name_char(key) && input_text_.size() < 64) {
        input_text_.push_back(static_cast<char>(key));
    }
    return {};
}

RuntimeControlMode RuntimeControls::mode() const {
    return mode_;
}

const std::string& RuntimeControls::input_text() const {
    return input_text_;
}

std::string RuntimeControls::prompt_text() const {
    if (mode_ == RuntimeControlMode::EnteringEnrollName) {
        return "Enroll name: " + input_text_;
    }
    if (mode_ == RuntimeControlMode::EnteringDeleteName) {
        return "Delete name: " + input_text_;
    }
    return "Ready";
}

}  // namespace ccamera
