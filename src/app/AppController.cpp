#include "app/AppController.hpp"

#include "app/AppRunCleanup.hpp"
#include "app/FrameRecognition.hpp"
#include "camera/CameraCapture.hpp"
#include "ui/PreviewWindow.hpp"

#include <opencv2/highgui.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <thread>
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
      recognizer_(config_.recognition_threshold),
      enrollment_session_(config_) {
    database_.load_or_create();
}

void AppController::apply_command(const Command& command, bool& running) {
    if (command.type == CommandType::Quit) {
        running = false;
    } else if (command.type == CommandType::Enroll) {
        enrollment_session_.start(command.name, Clock::now());
    } else if (command.type == CommandType::Delete) {
        database_.delete_person(command.name);
        database_.save();
    } else if (command.type == CommandType::List) {
        const auto snapshot = database_.snapshot();
        for (const auto& person : snapshot.people) {
            std::cout << person.name << " (" << person.templates.size() << " templates)\n";
        }
    }
}

void AppController::apply_commands(bool& running) {
    const CommandState commands = command_loop_.consume();

    if (!commands.commands.empty()) {
        for (const auto& command : commands.commands) {
            try {
                apply_command(command, running);
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
            apply_command({CommandType::Quit, {}}, running);
        }
        if (commands.enroll_name.has_value()) {
            apply_command({CommandType::Enroll, *commands.enroll_name}, running);
        }
        if (commands.delete_name.has_value()) {
            apply_command({CommandType::Delete, *commands.delete_name}, running);
        }
        if (commands.list_requested) {
            apply_command({CommandType::List, {}}, running);
        }
    } catch (const std::exception& error) {
        std::cerr << "Command failed: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "Command failed: unknown error\n";
    }
}

void AppController::handle_enrollment(
    const cv::Mat& image,
    const std::vector<FaceDetection>& faces,
    Clock::time_point now) {
    if (!enrollment_session_.active()) {
        return;
    }
    if (faces.size() != 1) {
        return;
    }

    try {
        const cv::Mat aligned = aligner_.align(image, faces.front());
        const FaceEmbedding sample = extractor_.extract(aligned);
        const EnrollmentUpdate update = enrollment_session_.add_sample(sample, now);
        if (update.status == EnrollmentStatus::Completed && update.embedding.has_value()) {
            database_.upsert_person(update.name, *update.embedding);
            database_.save();
        } else if (update.status == EnrollmentStatus::RejectedInconsistent) {
            std::cerr << "Enrollment sample rejected for " << update.name << ": inconsistent embedding\n";
        } else if (update.status == EnrollmentStatus::TimedOut) {
            std::cerr << "Enrollment timed out for " << update.name << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "Enrollment failed: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "Enrollment failed: unknown error\n";
    }
}

std::vector<RecognitionResult> AppController::process_frame(const Frame& frame) {
    const auto faces = detector_.detect(frame.image);

    handle_enrollment(frame.image, faces, Clock::now());

    const auto db_snapshot = database_.snapshot();
    return recognize_frame_faces(frame, faces, aligner_, extractor_, db_snapshot, recognizer_);
}

void AppController::recognition_loop() {
    while (recognition_running_) {
        const auto frame = frame_buffer_.latest();
        if (!frame.has_value() || frame->sequence == last_processed_sequence_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        last_processed_sequence_ = frame->sequence;
        const bool ok = run_recognition_step(recognition_results_, frame->sequence, [&] {
            return process_frame(*frame);
        });
        if (!ok) {
            const auto status = recognition_results_.status();
            std::cerr << "Recognition failed on frame " << status.sequence << ": " << status.last_error << '\n';
        }
    }
}

int AppController::run() {
    CameraCapture camera(config_, frame_buffer_);
    PreviewWindow preview("CCamera");

    camera.start();
    AppRunCleanup cleanup(
        [this] {
            recognition_running_ = false;
            if (recognition_thread_.joinable()) {
                recognition_thread_.join();
            }
        },
        [this] { command_loop_.stop(); },
        [&camera] { camera.stop(); });
    recognition_running_ = true;
    recognition_thread_ = std::thread(&AppController::recognition_loop, this);
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

            preview.show(
                frame->image,
                recognition_results_.results_for_sequence(frame->sequence),
                database_.snapshot(),
                fps);
        }

        for (const auto& command : preview.consume_commands()) {
            try {
                apply_command(command, running);
            } catch (const std::exception& error) {
                std::cerr << "Command failed: " << error.what() << '\n';
            }
        }

        const int key = cv::waitKey(1);
        const bool preview_was_accepting_text = preview.accepting_text_input();
        for (const auto& command : preview.handle_key(key)) {
            try {
                apply_command(command, running);
            } catch (const std::exception& error) {
                std::cerr << "Command failed: " << error.what() << '\n';
            }
        }
        if (!preview_was_accepting_text && (key == 'q' || key == 'Q' || key == 27)) {
            running = false;
        }
    }

    recognition_running_ = false;
    if (recognition_thread_.joinable()) {
        recognition_thread_.join();
    }
    command_loop_.stop();
    camera.stop();
    cleanup.dismiss();
    return 0;
}

}  // namespace ccamera
