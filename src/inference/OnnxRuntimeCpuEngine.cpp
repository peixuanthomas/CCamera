#include "inference/OnnxRuntimeCpuEngine.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>

namespace ccamera {
namespace {

#ifdef _WIN32
std::wstring to_model_path(const std::string& model_path) {
    return std::filesystem::path(model_path).wstring();
}
#else
std::string to_model_path(const std::string& model_path) {
    return model_path;
}
#endif

}  // namespace

OnnxRuntimeCpuEngine::OnnxRuntimeCpuEngine(const std::string& model_path)
    : env_(ORT_LOGGING_LEVEL_WARNING, "ccamera"),
      options_(),
      session_(nullptr),
      allocator_(),
      input_names_(),
      output_names_() {
    options_.SetIntraOpNumThreads(1);
    options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    const auto ort_model_path = to_model_path(model_path);
    session_ = Ort::Session(env_, ort_model_path.c_str(), options_);

    const std::size_t input_count = session_.GetInputCount();
    input_names_.reserve(input_count);
    for (std::size_t index = 0; index < input_count; ++index) {
        auto name = session_.GetInputNameAllocated(index, allocator_);
        input_names_.emplace_back(name.get());
    }

    const std::size_t output_count = session_.GetOutputCount();
    output_names_.reserve(output_count);
    for (std::size_t index = 0; index < output_count; ++index) {
        auto name = session_.GetOutputNameAllocated(index, allocator_);
        output_names_.emplace_back(name.get());
    }
}

TensorMap OnnxRuntimeCpuEngine::run(const TensorMap& inputs) {
    const Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<const char*> input_name_ptrs;
    input_name_ptrs.reserve(input_names_.size());

    std::vector<Ort::Value> input_tensors;
    input_tensors.reserve(input_names_.size());

    for (const auto& input_name : input_names_) {
        const auto input = inputs.find(input_name);
        if (input == inputs.end()) {
            throw std::runtime_error("missing required model input: " + input_name);
        }

        const Tensor& tensor = input->second;
        validate_tensor_shape_and_values(input_name, tensor);
        const std::size_t element_count = tensor.values.size();
        input_name_ptrs.push_back(input_name.c_str());
        input_tensors.push_back(Ort::Value::CreateTensor<float>(
            memory_info,
            const_cast<float*>(tensor.values.data()),
            element_count,
            tensor.shape.data(),
            tensor.shape.size()));
    }

    std::vector<const char*> output_name_ptrs;
    output_name_ptrs.reserve(output_names_.size());
    for (const auto& output_name : output_names_) {
        output_name_ptrs.push_back(output_name.c_str());
    }

    std::vector<Ort::Value> output_tensors = session_.Run(
        Ort::RunOptions{nullptr},
        input_name_ptrs.data(),
        input_tensors.data(),
        input_tensors.size(),
        output_name_ptrs.data(),
        output_name_ptrs.size());

    TensorMap outputs;
    for (std::size_t index = 0; index < output_tensors.size(); ++index) {
        const auto shape_info = output_tensors[index].GetTensorTypeAndShapeInfo();
        const std::vector<int64_t> shape = shape_info.GetShape();
        const std::size_t element_count = shape_info.GetElementCount();
        const float* data = output_tensors[index].GetTensorData<float>();

        Tensor tensor;
        tensor.shape = shape;
        tensor.values.assign(data, data + element_count);
        outputs.emplace(output_names_[index], std::move(tensor));
    }

    return outputs;
}

}  // namespace ccamera
