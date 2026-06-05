#include "camera/CameraCapture.hpp"

#include <opencv2/videoio.hpp>

#include <memory>
#include <utility>

namespace ccamera {
namespace {

class OpenCvCameraSource final : public CameraSource {
public:
    bool open(int camera_index) override {
        capture_.open(camera_index);
        return capture_.isOpened();
    }

    bool is_opened() const override {
        return capture_.isOpened();
    }

    void set(int property, double value) override {
        capture_.set(property, value);
    }

    bool read(cv::Mat& image) override {
        return capture_.read(image);
    }

private:
    cv::VideoCapture capture_;
};

CameraSourceFactory default_camera_source_factory() {
    return [] { return std::make_unique<OpenCvCameraSource>(); };
}

}  // namespace

CameraCapture::CameraCapture(AppConfig config, FrameBuffer& buffer)
    : CameraCapture(std::move(config), buffer, default_camera_source_factory()) {}

}  // namespace ccamera
