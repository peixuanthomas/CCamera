#pragma once

#include "app/CommandLoop.hpp"
#include "camera/FrameBuffer.hpp"
#include "core/Types.hpp"
#include "face/FaceAligner.hpp"
#include "face/FaceDatabase.hpp"
#include "face/FaceDetector.hpp"
#include "face/FaceFeatureExtractor.hpp"
#include "face/Recognizer.hpp"
#include "inference/OnnxRuntimeCpuEngine.hpp"

#include <memory>
#include <opencv2/core.hpp>
#include <optional>
#include <vector>

namespace ccamera {

class AppController {
public:
    explicit AppController(AppConfig config);
    int run();

private:
    std::vector<RecognitionResult> process_frame(const cv::Mat& image);
    void apply_commands(bool& running);
    void handle_enrollment(const cv::Mat& image, const std::vector<FaceDetection>& faces);

    AppConfig config_;
    FrameBuffer frame_buffer_;
    OnnxRuntimeCpuEngine detector_engine_;
    OnnxRuntimeCpuEngine recognizer_engine_;
    FaceDetector detector_;
    FaceAligner aligner_;
    FaceFeatureExtractor extractor_;
    FaceDatabase database_;
    Recognizer recognizer_;
    CommandLoop command_loop_;
    std::optional<std::string> enrollment_name_;
    std::vector<FaceEmbedding> enrollment_samples_;
};

}  // namespace ccamera
