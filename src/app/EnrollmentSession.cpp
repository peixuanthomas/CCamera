#include "app/EnrollmentSession.hpp"

#include "app/AppConfig.hpp"
#include "face/FaceFeatureExtractor.hpp"

#include <chrono>
#include <utility>

namespace ccamera {

EnrollmentSession::EnrollmentSession(AppConfig config) : config_(std::move(config)) {
    validate_app_config(config_);
}

void EnrollmentSession::start(const std::string& name, Clock::time_point now) {
    std::lock_guard<std::mutex> lock(mutex_);
    name_ = name;
    started_at_ = now;
    samples_.clear();
}

void EnrollmentSession::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    name_.reset();
    samples_.clear();
}

bool EnrollmentSession::active() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return name_.has_value();
}

std::size_t EnrollmentSession::sample_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return samples_.size();
}

EnrollmentUpdate EnrollmentSession::add_sample(const FaceEmbedding& sample, Clock::time_point now) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!name_.has_value()) {
        return {};
    }

    const std::string current_name = *name_;
    if (timed_out(now)) {
        name_.reset();
        samples_.clear();
        return {EnrollmentStatus::TimedOut, current_name, std::nullopt};
    }

    if (!consistent_with_existing(sample)) {
        return {EnrollmentStatus::RejectedInconsistent, current_name, std::nullopt};
    }

    samples_.push_back(sample);
    if (static_cast<int>(samples_.size()) < config_.enrollment_sample_count) {
        return {EnrollmentStatus::Accepted, current_name, std::nullopt};
    }

    FaceEmbedding averaged = average_embeddings(samples_);
    name_.reset();
    samples_.clear();
    return {EnrollmentStatus::Completed, current_name, averaged};
}

bool EnrollmentSession::timed_out(Clock::time_point now) const {
    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - started_at_);
    return elapsed.count() > config_.enrollment_timeout_seconds;
}

bool EnrollmentSession::consistent_with_existing(const FaceEmbedding& sample) const {
    if (samples_.empty()) {
        return true;
    }
    const FaceEmbedding averaged = average_embeddings(samples_);
    return cosine_similarity(sample.values, averaged.values) >= config_.enrollment_consistency_threshold;
}

}  // namespace ccamera
