#pragma once

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
#include <vector>

namespace ccamera {

class AppController {
public:
    explicit AppController(AppConfig config);
    int run();

private:
    std::vector<RecognitionResult> process_frame(const cv::Mat& image);

    AppConfig config_;
    FrameBuffer frame_buffer_;
    OnnxRuntimeCpuEngine detector_engine_;
    OnnxRuntimeCpuEngine recognizer_engine_;
    FaceDetector detector_;
    FaceAligner aligner_;
    FaceFeatureExtractor extractor_;
    FaceDatabase database_;
    Recognizer recognizer_;
};

}  // namespace ccamera
