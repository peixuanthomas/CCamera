#pragma once

#include "core/Types.hpp"
#include "face/FaceDatabase.hpp"

#include <string>
#include <vector>

namespace ccamera {

struct WebStatusSnapshot {
    bool running = false;
    double fps = 0.0;
    std::string status;
    std::string database_path;
    std::vector<RecognitionResult> results;
    FaceDatabaseSnapshot database;
};

std::string serialize_web_status(const WebStatusSnapshot& snapshot);

}  // namespace ccamera
