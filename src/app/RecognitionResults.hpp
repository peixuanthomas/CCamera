#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace ccamera {

struct RecognitionStatus {
    std::uint64_t sequence = 0;
    std::string last_error;
};

class RecognitionResultsStore {
public:
    void store_results(std::uint64_t sequence, std::vector<RecognitionResult> results);
    void store_error(std::uint64_t sequence, const std::string& error);
    std::vector<RecognitionResult> results_for_sequence(std::uint64_t sequence) const;
    // Most recent successful results regardless of sequence. The live preview shows
    // the newest captured frame while recognition runs a few frames behind, so the
    // overlay must use the latest available results instead of an exact sequence match.
    std::vector<RecognitionResult> latest_results() const;
    RecognitionStatus status() const;

private:
    mutable std::mutex mutex_;
    std::uint64_t sequence_ = 0;
    std::vector<RecognitionResult> results_;
    std::string last_error_;
};

bool run_recognition_step(
    RecognitionResultsStore& store,
    std::uint64_t sequence,
    const std::function<std::vector<RecognitionResult>()>& process);

}  // namespace ccamera
