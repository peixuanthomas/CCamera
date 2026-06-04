#pragma once

#include <vector>

namespace ccamera {

float dot_product(const std::vector<float>& a, const std::vector<float>& b);
float l2_norm(const std::vector<float>& values);
std::vector<float> l2_normalized(const std::vector<float>& values);
float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);

}  // namespace ccamera
