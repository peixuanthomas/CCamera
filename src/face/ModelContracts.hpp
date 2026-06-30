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

struct InsightFaceArcFaceModelContract {
    std::string input_name = "data";
    std::string output_name = "fc1";
    std::string input_layout =
        "NCHW float32 aligned 112x112 face tensor, batch 1, RGB channel order, normalized with mean 127.5 and std 127.5";
    std::string output_layout = "single 512-D ArcFace embedding tensor";
    int input_width = 112;
    int input_height = 112;
    int embedding_dimensions = 512;
};

inline DetectorModelContract detector_model_contract() {
    return {};
}

inline RecognizerModelContract recognizer_model_contract() {
    return {};
}

inline InsightFaceArcFaceModelContract insightface_arcface_model_contract() {
    return {};
}

}  // namespace ccamera
