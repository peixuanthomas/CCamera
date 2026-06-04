#pragma once

#include "inference/InferenceEngine.hpp"

#include <onnxruntime_cxx_api.h>

#include <memory>
#include <string>
#include <vector>

namespace ccamera {

class OnnxRuntimeCpuEngine final : public InferenceEngine {
public:
    explicit OnnxRuntimeCpuEngine(const std::string& model_path);

    TensorMap run(const TensorMap& inputs) override;

private:
    Ort::Env env_;
    Ort::SessionOptions options_;
    Ort::Session session_;
    Ort::AllocatorWithDefaultOptions allocator_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
};

}  // namespace ccamera
