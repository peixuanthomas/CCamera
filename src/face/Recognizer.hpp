#pragma once

#include "core/Types.hpp"
#include "face/FaceDatabase.hpp"

namespace ccamera {

struct MatchResult {
    std::string label = "Unknown";
    float similarity = 0.0f;
    bool known = false;
};

class Recognizer {
public:
    explicit Recognizer(float threshold);
    MatchResult match(const FaceEmbedding& embedding, const FaceDatabaseSnapshot& database) const;

private:
    float threshold_;
};

}  // namespace ccamera
