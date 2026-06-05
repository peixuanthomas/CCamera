#pragma once

#include "core/Math.hpp"
#include "core/Types.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ccamera {

enum class EnrollmentStatus {
    Inactive,
    Accepted,
    RejectedInconsistent,
    Completed,
    TimedOut,
};

struct EnrollmentUpdate {
    EnrollmentStatus status = EnrollmentStatus::Inactive;
    std::string name;
    std::optional<FaceEmbedding> embedding;
};

class EnrollmentSession {
public:
    explicit EnrollmentSession(AppConfig config);

    void start(const std::string& name, Clock::time_point now);
    void reset();
    bool active() const;
    std::size_t sample_count() const;
    EnrollmentUpdate add_sample(const FaceEmbedding& sample, Clock::time_point now);

private:
    bool timed_out(Clock::time_point now) const;
    bool consistent_with_existing(const FaceEmbedding& sample) const;

    AppConfig config_;
    mutable std::mutex mutex_;
    std::optional<std::string> name_;
    Clock::time_point started_at_{};
    std::vector<FaceEmbedding> samples_;
};

}  // namespace ccamera
