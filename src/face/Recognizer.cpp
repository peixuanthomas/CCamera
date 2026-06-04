#include "face/Recognizer.hpp"

#include "core/Math.hpp"

#include <limits>

namespace ccamera {

Recognizer::Recognizer(float threshold) : threshold_(threshold) {}

MatchResult Recognizer::match(const FaceEmbedding& embedding, const FaceDatabaseSnapshot& database) const {
    MatchResult best;
    best.similarity = -std::numeric_limits<float>::infinity();

    for (const auto& person : database.people) {
        for (const auto& templ : person.templates) {
            const float score = cosine_similarity(embedding.values, templ);
            if (score > best.similarity) {
                best.similarity = score;
                best.label = person.name;
            }
        }
    }

    if (best.similarity == -std::numeric_limits<float>::infinity()) {
        best.similarity = 0.0f;
        return best;
    }

    best.known = best.similarity >= threshold_;
    if (!best.known) {
        best.label = "Unknown";
    }
    return best;
}

}  // namespace ccamera
