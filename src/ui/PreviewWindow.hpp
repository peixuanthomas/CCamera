#pragma once

#include "core/Types.hpp"
#include "face/FaceDatabase.hpp"
#include "ui/RuntimeControls.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace ccamera {

class PreviewWindow {
public:
    explicit PreviewWindow(std::string name);
    void show(
        const cv::Mat& frame,
        const std::vector<RecognitionResult>& results,
        const FaceDatabaseSnapshot& database,
        double fps);
    std::vector<Command> consume_commands();
    std::vector<Command> handle_key(int key);
    bool accepting_text_input() const;
    static void draw_overlay(cv::Mat& frame, const std::vector<RecognitionResult>& results);

private:
    static void on_mouse(int event, int x, int y, int flags, void* userdata);
    void draw_controls(cv::Mat& frame, const FaceDatabaseSnapshot& database, double fps);

    std::string name_;
    RuntimeControls controls_;
    RuntimeControlsLayout latest_layout_{};
    std::vector<Command> pending_commands_;
};

}  // namespace ccamera
