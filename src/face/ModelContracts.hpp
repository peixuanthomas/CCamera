#pragma once

#include <string>

namespace ccamera {

struct DetectorModelContract {
    std::string input_name = "image";
    std::string output_name = "detections";
    std::string input_layout = "NCHW float32 image tensor, batch 1, BGR channel order from OpenCV frames";
    int detection_columns = 15;
};

struct RecognizerModelContract {
    std::string input_name = "face";
    std::string output_name = "embedding";
    std::string input_layout = "NCHW float32 aligned face tensor, batch 1, BGR channel order from OpenCV crops";
};

inline DetectorModelContract detector_model_contract() {
    return {};
}

inline RecognizerModelContract recognizer_model_contract() {
    return {};
}

}  // namespace ccamera
