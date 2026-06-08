#pragma once

#include "app/CommandLoop.hpp"
#include "app/EnrollmentSession.hpp"
#include "app/RecognitionResults.hpp"
#include "camera/FrameBuffer.hpp"
#include "core/Types.hpp"
#include "face/FaceAligner.hpp"
#include "face/FaceDatabase.hpp"
#include "face/FaceDetector.hpp"
#include "face/FaceFeatureExtractor.hpp"
#include "face/FaceServices.hpp"
#include "face/Recognizer.hpp"
#include "inference/OnnxRuntimeCpuEngine.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <opencv2/core.hpp>
#include <optional>
#include <thread>
#include <vector>

namespace ccamera {

class AppController {
public:
    explicit AppController(AppConfig config);
    int run();

private:
    std::vector<RecognitionResult> process_frame(const Frame& frame);
    void recognition_loop();
    void apply_command(const Command& command, bool& running);
    void apply_commands(bool& running);
    void handle_enrollment(const cv::Mat& image, const std::vector<FaceDetection>& faces, Clock::time_point now);

    AppConfig config_;
    FrameBuffer frame_buffer_;
    std::unique_ptr<OnnxRuntimeCpuEngine> detector_engine_;
    std::unique_ptr<OnnxRuntimeCpuEngine> recognizer_engine_;
    std::unique_ptr<FaceDetectionService> detector_;
    FaceAligner aligner_;
    std::unique_ptr<FaceEmbeddingService> extractor_;
    FaceDatabase database_;
    Recognizer recognizer_;
    CommandLoop command_loop_;
    EnrollmentSession enrollment_session_;
    std::atomic<bool> recognition_running_{false};
    std::thread recognition_thread_;
    RecognitionResultsStore recognition_results_;
    std::uint64_t last_processed_sequence_ = 0;
};

}  // namespace ccamera
