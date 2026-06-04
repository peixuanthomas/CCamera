#include "face/FaceFeatureExtractor.hpp"

#include "core/Math.hpp"

#include <stdexcept>

namespace ccamera {
namespace {

std::size_t validate_embedding_output(const Tensor& tensor) {
    if (tensor.shape.size() == 1) {
        if (tensor.shape[0] <= 0) {
            throw std::runtime_error("recognizer output 'embedding' dimension must be positive");
        }

        const auto dimension = static_cast<std::size_t>(tensor.shape[0]);
        if (tensor.values.size() != dimension) {
            throw std::runtime_error("recognizer output 'embedding' values do not match shape");
        }
        return dimension;
    }

    if (tensor.shape.size() == 2) {
        if (tensor.shape[0] != 1) {
            throw std::runtime_error("recognizer output 'embedding' batch size must be 1");
        }
        if (tensor.shape[1] <= 0) {
            throw std::runtime_error("recognizer output 'embedding' dimension must be positive");
        }

        const auto dimension = static_cast<std::size_t>(tensor.shape[1]);
        if (tensor.values.size() != dimension) {
            throw std::runtime_error("recognizer output 'embedding' values do not match shape");
        }
        return dimension;
    }

    throw std::runtime_error("recognizer output 'embedding' must have shape [N] or [1, N]");
}

}  // namespace

FaceFeatureExtractor::FaceFeatureExtractor(InferenceEngine& engine) : engine_(engine) {}

FaceEmbedding FaceFeatureExtractor::extract(const cv::Mat& aligned_face) {
    cv::Mat float_face;
    aligned_face.convertTo(float_face, CV_32F);

    Tensor input;
    input.shape = {1, aligned_face.channels(), aligned_face.rows, aligned_face.cols};
    input.values.resize(static_cast<std::size_t>(aligned_face.total() * aligned_face.channels()));

    const int channels = aligned_face.channels();
    const int face_rows = aligned_face.rows;
    const int face_cols = aligned_face.cols;
    const auto plane_size = static_cast<std::size_t>(face_rows * face_cols);
    for (int y = 0; y < face_rows; ++y) {
        const float* row = float_face.ptr<float>(y);
        for (int x = 0; x < face_cols; ++x) {
            for (int channel = 0; channel < channels; ++channel) {
                const auto output_index =
                    static_cast<std::size_t>(channel) * plane_size + static_cast<std::size_t>(y * face_cols + x);
                input.values[output_index] = row[x * channels + channel];
            }
        }
    }

    const auto outputs = engine_.run({{"face", input}});
    const auto found = outputs.find("embedding");
    if (found == outputs.end()) {
        throw std::runtime_error("recognizer output 'embedding' is missing");
    }
    validate_embedding_output(found->second);

    FaceEmbedding embedding;
    embedding.values = l2_normalized(found->second.values);
    return embedding;
}

FaceEmbedding average_embeddings(const std::vector<FaceEmbedding>& embeddings) {
    if (embeddings.empty()) {
        throw std::invalid_argument("cannot average zero embeddings");
    }

    const auto dimension = embeddings.front().values.size();
    std::vector<float> sum(dimension, 0.0f);
    for (const auto& embedding : embeddings) {
        if (embedding.values.size() != dimension) {
            throw std::invalid_argument("embedding dimension mismatch during averaging");
        }
        for (std::size_t i = 0; i < dimension; ++i) {
            sum[i] += embedding.values[i];
        }
    }

    for (float& value : sum) {
        value /= static_cast<float>(embeddings.size());
    }
    return {l2_normalized(sum)};
}

}  // namespace ccamera
