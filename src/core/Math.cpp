#include "core/Math.hpp"

#include <cmath>
#include <stdexcept>

namespace ccamera {

float dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("dot_product requires equal vector sizes");
    }
    float result = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

float l2_norm(const std::vector<float>& values) {
    float sum = 0.0f;
    for (float value : values) {
        sum += value * value;
    }
    return std::sqrt(sum);
}

std::vector<float> l2_normalized(const std::vector<float>& values) {
    const float norm = l2_norm(values);
    if (norm <= 1.0e-12f) {
        throw std::invalid_argument("cannot normalize a zero-norm vector");
    }
    std::vector<float> normalized;
    normalized.reserve(values.size());
    for (float value : values) {
        normalized.push_back(value / norm);
    }
    return normalized;
}

float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    return dot_product(l2_normalized(a), l2_normalized(b));
}

}  // namespace ccamera
