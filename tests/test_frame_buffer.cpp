#include "camera/FrameBuffer.hpp"

#include <cassert>
#include <chrono>
#include <opencv2/core.hpp>

int main() {
    ccamera::FrameBuffer buffer;
    assert(!buffer.latest().has_value());

    ccamera::Frame first;
    first.sequence = 1;
    first.image = cv::Mat::zeros(2, 2, CV_8UC3);
    buffer.publish(first);

    ccamera::Frame second;
    second.sequence = 2;
    second.captured_at = ccamera::Clock::time_point{std::chrono::milliseconds(123)};
    second.image = cv::Mat::ones(2, 2, CV_8UC3);
    second.image.setTo(cv::Scalar(1, 1, 1));
    buffer.publish(second);
    second.image.setTo(cv::Scalar(7, 7, 7));

    auto latest = buffer.latest();
    assert(latest.has_value());
    assert(latest->sequence == 2);
    assert(latest->captured_at == ccamera::Clock::time_point{std::chrono::milliseconds(123)});
    assert(latest->image.rows == 2);
    assert(latest->image.cols == 2);
    const auto source_mutation_pixel = latest->image.at<cv::Vec3b>(0, 0);
    assert(source_mutation_pixel[0] == 1);
    assert(source_mutation_pixel[1] == 1);
    assert(source_mutation_pixel[2] == 1);

    latest->image.setTo(cv::Scalar(9, 9, 9));

    const auto latest_again = buffer.latest();
    assert(latest_again.has_value());
    const auto returned_mutation_pixel = latest_again->image.at<cv::Vec3b>(0, 0);
    assert(returned_mutation_pixel[0] == 1);
    assert(returned_mutation_pixel[1] == 1);
    assert(returned_mutation_pixel[2] == 1);

    return 0;
}
