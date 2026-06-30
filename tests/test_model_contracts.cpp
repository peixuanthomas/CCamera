#include "face/ModelContracts.hpp"
#include "inference/InferenceEngine.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

template <typename Callable>
void expect_contract_error(Callable&& callable, const std::string& expected_message) {
    try {
        callable();
    } catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(expected_message) != std::string::npos);
        return;
    }
    assert(false);
}

}  // namespace

int main() {
    const auto detector = ccamera::detector_model_contract();
    assert(detector.input_name == "image");
    assert(detector.output_name == "detections");
    assert(detector.detection_columns == 15);
    assert(detector.input_layout.find("NCHW") != std::string::npos);

    const auto recognizer = ccamera::recognizer_model_contract();
    assert(recognizer.input_name == "face");
    assert(recognizer.output_name == "embedding");
    assert(recognizer.input_layout.find("NCHW") != std::string::npos);
    assert(recognizer.input_layout.find("BGR") != std::string::npos);

    const auto insightface = ccamera::insightface_arcface_model_contract();
    assert(insightface.input_name == "data");
    assert(insightface.output_name == "fc1");
    assert(insightface.input_width == 112);
    assert(insightface.input_height == 112);
    assert(insightface.embedding_dimensions == 512);
    assert(insightface.input_layout.find("RGB") != std::string::npos);

    const ccamera::Tensor valid{{1, 3, 2, 2}, std::vector<float>(12, 1.0f)};
    ccamera::validate_tensor_shape_and_values("image", valid);
    assert(ccamera::tensor_element_count(valid.shape) == 12);

    expect_contract_error(
        [] {
            const ccamera::Tensor bad{{1, 0, 2, 2}, {}};
            ccamera::validate_tensor_shape_and_values("image", bad);
        },
        "positive");

    expect_contract_error(
        [] {
            const ccamera::Tensor bad{{1, 3, 2, 2}, std::vector<float>(11, 1.0f)};
            ccamera::validate_tensor_shape_and_values("image", bad);
        },
        "does not match");

    return 0;
}
