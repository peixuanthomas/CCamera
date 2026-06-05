#include "app/EnrollmentSession.hpp"

#include <cassert>
#include <cmath>

namespace {

bool close_to(float actual, float expected) {
    return std::fabs(actual - expected) < 1.0e-5f;
}

}  // namespace

int main() {
    ccamera::AppConfig config;
    config.enrollment_sample_count = 3;
    config.enrollment_consistency_threshold = 0.9f;
    config.enrollment_timeout_seconds = 2;

    ccamera::EnrollmentSession session(config);
    const auto start = ccamera::Clock::now();
    session.start("Alice", start);

    auto first = session.add_sample({{1.0f, 0.0f}}, start);
    assert(first.status == ccamera::EnrollmentStatus::Accepted);
    assert(session.sample_count() == 1);

    auto inconsistent = session.add_sample({{0.0f, 1.0f}}, start + std::chrono::seconds(1));
    assert(inconsistent.status == ccamera::EnrollmentStatus::RejectedInconsistent);
    assert(session.active());
    assert(session.sample_count() == 1);

    auto second = session.add_sample({{0.98f, 0.01f}}, start + std::chrono::seconds(1));
    assert(second.status == ccamera::EnrollmentStatus::Accepted);
    assert(session.sample_count() == 2);

    auto complete = session.add_sample({{1.0f, 0.0f}}, start + std::chrono::seconds(1));
    assert(complete.status == ccamera::EnrollmentStatus::Completed);
    assert(complete.name == "Alice");
    assert(complete.embedding.has_value());
    assert(complete.embedding->values.size() == 2);
    assert(close_to(ccamera::l2_norm(complete.embedding->values), 1.0f));
    assert(!session.active());

    session.start("Bob", start);
    auto timed_out = session.add_sample({{1.0f, 0.0f}}, start + std::chrono::seconds(3));
    assert(timed_out.status == ccamera::EnrollmentStatus::TimedOut);
    assert(!timed_out.embedding.has_value());
    assert(!session.active());

    return 0;
}
