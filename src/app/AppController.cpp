#include "app/AppController.hpp"

#include "camera/CameraCapture.hpp"
#include "ui/PreviewWindow.hpp"

#include <opencv2/highgui.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <utility>

namespace ccamera {

AppController::AppController(AppConfig config)
    : config_(std::move(config)),
      detector_engine_(config_.detector_model),
      recognizer_engine_(config_.recognizer_model),
      detector_(detector_engine_, config_.detection_threshold),
      aligner_(),
      extractor_(recognizer_engine_),
      database_(config_.database_path),
      recognizer_(config_.recognition_threshold) {
    database_.load_or_create();
}

void AppController::apply_commands(bool& running) {
    const CommandState commands = command_loop_.consume();

    const auto apply_one = [this, &running](const Command& command) {
        if (command.type == CommandType::Quit) {
            running = false;
        } else if (command.type == CommandType::Enroll) {
            enrollment_name_ = command.name;
            enrollment_samples_.clear();
        } else if (command.type == CommandType::Delete) {
            database_.delete_person(command.name);
            database_.save();
        } else if (command.type == CommandType::List) {
            const auto snapshot = database_.snapshot();
            for (const auto& person : snapshot.people) {
                std::cout << person.name << " (" << person.templates.size() << " templates)\n";
            }
        }
    };

    if (!commands.commands.empty()) {
        for (const auto& command : commands.commands) {
            try {
                apply_one(command);
            } catch (const std::exception& error) {
                std::cerr << "Command failed: " << error.what() << '\n';
            } catch (...) {
                std::cerr << "Command failed: unknown error\n";
            }
        }
        return;
    }

    try {
        if (commands.quit_requested) {
            apply_one({CommandType::Quit, {}});
        }
        if (commands.enroll_name.has_value()) {
            apply_one({CommandType::Enroll, *commands.enroll_name});
        }
        if (commands.delete_name.has_value()) {
            apply_one({CommandType::Delete, *commands.delete_name});
        }
        if (commands.list_requested) {
            apply_one({CommandType::List, {}});
        }
    } catch (const std::exception& error) {
        std::cerr << "Command failed: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "Command failed: unknown error\n";
    }
}

void AppController::handle_enrollment(const cv::Mat& image, const std::vector<FaceDetection>& faces) {
    if (!enrollment_name_.has_value()) {
        return;
    }
    if (faces.size() != 1) {
        return;
    }

    try {
        const cv::Mat aligned = aligner_.align(image, faces.front());
        enrollment_samples_.push_back(extractor_.extract(aligned));

        if (static_cast<int>(enrollment_samples_.size()) >= config_.enrollment_sample_count) {
            FaceEmbedding averaged = average_embeddings(enrollment_samples_);
            database_.upsert_person(*enrollment_name_, averaged);
            database_.save();
            enrollment_name_.reset();
            enrollment_samples_.clear();
        }
    } catch (const std::exception& error) {
        std::cerr << "Enrollment failed for " << *enrollment_name_ << ": " << error.what() << '\n';
        enrollment_samples_.clear();
    } catch (...) {
        std::cerr << "Enrollment failed for " << *enrollment_name_ << ": unknown error\n";
        enrollment_samples_.clear();
    }
}

std::vector<RecognitionResult> AppController::process_frame(const cv::Mat& image) {
    std::vector<RecognitionResult> results;
    const auto faces = detector_.detect(image);

    handle_enrollment(image, faces);

    const auto db_snapshot = database_.snapshot();

    for (const auto& face : faces) {
        RecognitionResult result;
        result.detection = face;
        try {
            const cv::Mat aligned = aligner_.align(image, face);
            const FaceEmbedding embedding = extractor_.extract(aligned);
            const MatchResult match = recognizer_.match(embedding, db_snapshot);
            result.label = match.label;
            result.similarity = match.similarity;
            result.known = match.known;
        } catch (const std::exception&) {
            result.label = "Invalid face";
            result.similarity = 0.0f;
            result.known = false;
        }
        results.push_back(std::move(result));
    }

    return results;
}

int AppController::run() {
    CameraCapture camera(config_, frame_buffer_);
    PreviewWindow preview("CCamera");

    camera.start();
    command_loop_.start();

    bool running = true;
    auto last_tick = Clock::now();
    double fps = 0.0;

    while (running) {
        apply_commands(running);

        const auto frame = frame_buffer_.latest();
        if (frame.has_value()) {
            const auto now = Clock::now();
            const double seconds = std::chrono::duration<double>(now - last_tick).count();
            if (seconds > 0.0) {
                fps = 1.0 / seconds;
            }
            last_tick = now;

            const auto results = process_frame(frame->image);
            preview.show(frame->image, results, fps);
        }

        const int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {
            running = false;
        }
    }

    command_loop_.stop();
    camera.stop();
    return 0;
}

}  // namespace ccamera
