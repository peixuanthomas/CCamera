#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ccamera {

struct Tensor {
    std::vector<int64_t> shape;
    std::vector<float> values;
};

using TensorMap = std::unordered_map<std::string, Tensor>;

class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;
    virtual TensorMap run(const TensorMap& inputs) = 0;
};

}  // namespace ccamera
