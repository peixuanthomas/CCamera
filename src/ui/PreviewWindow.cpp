#include "ui/PreviewWindow.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <sstream>
#include <utility>

namespace ccamera {
namespace {

void draw_button(cv::Mat& frame, const cv::Rect& rect, const std::string& label, const cv::Scalar& color) {
    cv::rectangle(frame, rect, color, cv::FILLED, cv::LINE_AA);
    cv::rectangle(frame, rect, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

    int baseline = 0;
    const auto text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.55, 1, &baseline);
    const cv::Point origin(
        rect.x + std::max(8, (rect.width - text_size.width) / 2),
        rect.y + (rect.height + text_size.height) / 2 - 3);
    cv::putText(frame, label, origin, cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
}

void draw_text(cv::Mat& frame, const std::string& text, cv::Point origin, double scale = 0.5) {
    cv::putText(frame, text, origin, cv::FONT_HERSHEY_SIMPLEX, scale, cv::Scalar(245, 245, 245), 1, cv::LINE_AA);
}

}  // namespace

PreviewWindow::PreviewWindow(std::string name) : name_(std::move(name)) {
    cv::namedWindow(name_, cv::WINDOW_NORMAL);
    cv::setMouseCallback(name_, &PreviewWindow::on_mouse, this);
}

void PreviewWindow::show(
    const cv::Mat& frame,
    const std::vector<RecognitionResult>& results,
    const FaceDatabaseSnapshot& database,
    double fps) {
    cv::Mat output = frame.clone();
    draw_overlay(output, results);
    draw_controls(output, database, fps);

    std::ostringstream text;
    text << "FPS: " << static_cast<int>(fps);
    cv::putText(output, text.str(), {12, 28}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);

    cv::imshow(name_, output);
}

std::vector<Command> PreviewWindow::consume_commands() {
    std::vector<Command> commands;
    commands.swap(pending_commands_);
    return commands;
}

std::vector<Command> PreviewWindow::handle_key(int key) {
    if (key < 0) {
        return {};
    }
    return controls_.key(key);
}

bool PreviewWindow::accepting_text_input() const {
    return controls_.mode() != RuntimeControlMode::Idle;
}

void PreviewWindow::on_mouse(int event, int x, int y, int, void* userdata) {
    if (event != cv::EVENT_LBUTTONDOWN || userdata == nullptr) {
        return;
    }
    auto* window = static_cast<PreviewWindow*>(userdata);
    std::vector<Command> commands = window->controls_.click(x, y, window->latest_layout_);
    window->pending_commands_.insert(window->pending_commands_.end(), commands.begin(), commands.end());
}

void PreviewWindow::draw_controls(cv::Mat& frame, const FaceDatabaseSnapshot& database, double fps) {
    latest_layout_ = controls_.layout_for(frame.size());
    cv::Mat overlay = frame.clone();
    cv::rectangle(overlay, latest_layout_.panel, cv::Scalar(32, 36, 42), cv::FILLED);
    cv::addWeighted(overlay, 0.82, frame, 0.18, 0.0, frame);

    const int left = latest_layout_.panel.x + 16;
    int y = 28;
    draw_text(frame, "CCamera", {left, y}, 0.68);
    y += 26;

    std::ostringstream fps_text;
    fps_text << "FPS " << static_cast<int>(fps);
    draw_text(frame, fps_text.str(), {left, y});

    draw_button(frame, latest_layout_.enroll_button, "Enroll", cv::Scalar(54, 125, 84));
    draw_button(frame, latest_layout_.delete_button, "Delete", cv::Scalar(121, 79, 62));
    draw_button(frame, latest_layout_.list_button, "List", cv::Scalar(70, 90, 128));
    draw_button(frame, latest_layout_.quit_button, "Quit", cv::Scalar(86, 86, 92));

    y = latest_layout_.quit_button.y + latest_layout_.quit_button.height + 28;
    draw_text(frame, controls_.prompt_text(), {left, y});
    y += 28;
    draw_text(frame, "Enrolled", {left, y}, 0.58);
    y += 24;

    const int bottom = latest_layout_.panel.y + latest_layout_.panel.height - 12;
    if (database.people.empty()) {
        draw_text(frame, "None", {left, y});
        return;
    }

    int shown = 0;
    for (const auto& person : database.people) {
        if (y > bottom - 20) {
            draw_text(frame, "...", {left, y});
            break;
        }
        std::ostringstream person_text;
        person_text << person.name << " (" << person.templates.size() << ")";
        draw_text(frame, person_text.str(), {left, y});
        y += 22;
        ++shown;
    }
    (void)shown;
}

}  // namespace ccamera
