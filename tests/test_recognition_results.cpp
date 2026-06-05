#include "app/RecognitionResults.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

int main() {
    ccamera::RecognitionResultsStore store;

    const bool failed = ccamera::run_recognition_step(store, 7, []() -> std::vector<ccamera::RecognitionResult> {
        throw std::runtime_error("detector failed");
    });
    assert(!failed);
    assert(store.results_for_sequence(7).empty());
    assert(store.status().last_error.find("detector failed") != std::string::npos);

    ccamera::RecognitionResult result;
    result.frame_sequence = 8;
    result.label = "Alice";
    const bool succeeded = ccamera::run_recognition_step(store, 8, [&] {
        return std::vector<ccamera::RecognitionResult>{result};
    });
    assert(succeeded);
    assert(store.status().last_error.empty());
    assert(store.results_for_sequence(8).size() == 1);
    assert(store.results_for_sequence(8)[0].frame_sequence == 8);
    assert(store.results_for_sequence(9).empty());

    return 0;
}
