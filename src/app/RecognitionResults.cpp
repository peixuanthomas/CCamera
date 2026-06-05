#include "app/RecognitionResults.hpp"

#include <exception>

namespace ccamera {

void RecognitionResultsStore::store_results(std::uint64_t sequence, std::vector<RecognitionResult> results) {
    for (auto& result : results) {
        result.frame_sequence = sequence;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    sequence_ = sequence;
    results_ = std::move(results);
    last_error_.clear();
}

void RecognitionResultsStore::store_error(std::uint64_t sequence, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    sequence_ = sequence;
    results_.clear();
    last_error_ = error;
}

std::vector<RecognitionResult> RecognitionResultsStore::results_for_sequence(std::uint64_t sequence) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sequence_ != sequence || !last_error_.empty()) {
        return {};
    }
    return results_;
}

RecognitionStatus RecognitionResultsStore::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {sequence_, last_error_};
}

bool run_recognition_step(
    RecognitionResultsStore& store,
    std::uint64_t sequence,
    const std::function<std::vector<RecognitionResult>()>& process) {
    try {
        store.store_results(sequence, process());
        return true;
    } catch (const std::exception& error) {
        store.store_error(sequence, error.what());
    } catch (...) {
        store.store_error(sequence, "unknown recognition error");
    }
    return false;
}

}  // namespace ccamera
