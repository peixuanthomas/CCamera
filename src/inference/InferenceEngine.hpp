#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ccamera {

struct Tensor {
    std::vector<int64_t> shape;
    std::vector<float> values;
};

using TensorMap = std::unordered_map<std::string, Tensor>;

inline std::size_t tensor_element_count(const std::vector<int64_t>& shape) {
    if (shape.empty()) {
        throw std::runtime_error("tensor shape must not be empty");
    }

    std::size_t count = 1;
    for (const int64_t dimension : shape) {
        if (dimension <= 0) {
            throw std::runtime_error("tensor dimensions must be positive");
        }
        const auto size_dimension = static_cast<std::size_t>(dimension);
        if (count > std::numeric_limits<std::size_t>::max() / size_dimension) {
            throw std::runtime_error("tensor element count overflow");
        }
        count *= size_dimension;
    }
    return count;
}

inline void validate_tensor_shape_and_values(const std::string& name, const Tensor& tensor) {
    const std::size_t expected_values = tensor_element_count(tensor.shape);
    if (tensor.values.size() != expected_values) {
        throw std::runtime_error("tensor '" + name + "' value count does not match shape");
    }
}

class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;
    virtual TensorMap run(const TensorMap& inputs) = 0;
};

}  // namespace ccamera
