#include "app/AppController.hpp"

#include "app/AppRunCleanup.hpp"
#include "app/FrameRecognition.hpp"
#include "camera/CameraCapture.hpp"
#include "face/OpenCvZooFaceDetector.hpp"
#include "face/OpenCvZooFaceFeatureExtractor.hpp"
#include "ui/PreviewWindow.hpp"
#include "web/MjpegStream.hpp"
#include "web/WebStatus.hpp"

#include <opencv2/highgui.hpp>

#include <chrono>
#include <exception>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace ccamera {
namespace {

struct FaceBackendBundle {
    std::unique_ptr<OnnxRuntimeCpuEngine> detector_engine;
    std::unique_ptr<OnnxRuntimeCpuEngine> recognizer_engine;
    std::unique_ptr<FaceDetectionService> detector;
    std::unique_ptr<FaceEmbeddingService> extractor;
};

FaceBackendBundle make_face_backend(const AppConfig& config) {
    FaceBackendBundle bundle;
    if (config.inference_backend == "opencv_zoo") {
        bundle.detector = std::make_unique<OpenCvZooFaceDetector>(
            config.detector_model,
            config.detection_threshold);
        bundle.extractor = std::make_unique<OpenCvZooFaceFeatureExtractor>(
            config.recognizer_model);
        return bundle;
    }

    if (config.inference_backend == "onnxruntime_cpu") {
        bundle.detector_engine = std::make_unique<OnnxRuntimeCpuEngine>(config.detector_model);
        bundle.recognizer_engine = std::make_unique<OnnxRuntimeCpuEngine>(config.recognizer_model);
        bundle.detector = std::make_unique<FaceDetector>(*bundle.detector_engine, config.detection_threshold);
        bundle.extractor = std::make_unique<FaceFeatureExtractor>(*bundle.recognizer_engine);
        return bundle;
    }

    throw std::invalid_argument("unknown inference_backend: " + config.inference_backend);
}

}  // namespace

AppController::AppController(AppConfig config)
    : config_(std::move(config)),
      aligner_(),
      database_(config_.database_path),
      recognizer_(config_.recognition_threshold),
      enrollment_session_(config_) {
    FaceBackendBundle backend = make_face_backend(config_);
    detector_engine_ = std::move(backend.detector_engine);
    recognizer_engine_ = std::move(backend.recognizer_engine);
    detector_ = std::move(backend.detector);
    extractor_ = std::move(backend.extractor);
    database_.load_or_create();
}

void AppController::set_status_message(const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_message_ = message;
}

std::string AppController::status_message() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_message_;
}

WebStatusSnapshot AppController::web_status_snapshot() const {
    WebStatusSnapshot snapshot;
    snapshot.running = recognition_running_;
    snapshot.fps = fps_;
    snapshot.status = status_message();
    snapshot.database_path = config_.database_path;
    snapshot.results = recognition_results_.latest_results();
    snapshot.database = database_.snapshot();
    return snapshot;
}

std::vector<unsigned char> AppController::web_stream_frame() const {
    const auto frame = frame_buffer_.latest();
    const auto results = recognition_results_.latest_results();
    const std::string status = status_message();
    if (frame.has_value()) {
        return format_mjpeg_part(encode_overlay_jpeg(frame->image, results, status));
    }
    return format_mjpeg_part(encode_overlay_jpeg(
        make_web_fallback_frame({config_.frame_width, config_.frame_height}, status),
        {},
        status));
}

void AppController::apply_command(const Command& command, bool& running) {
    if (command.type == CommandType::Quit) {
        running = false;
    } else if (command.type == CommandType::Enroll) {
        enrollment_session_.start(command.name, Clock::now());
        set_status_message("Enrolling " + command.name + ": hold still, face the camera");
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
    for (const auto& command : web_commands_.consume()) {
        try {
            apply_command(command, running);
        } catch (const std::exception& error) {
            std::cerr << "Web command failed: " << error.what() << '\n';
            set_status_message(std::string("Web command failed: ") + error.what());
        }
    }

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
        set_status_message(
            "Enrolling: show exactly one face (" + std::to_string(faces.size()) + " detected)");
        return;
    }

    try {
        const FaceEmbedding sample = extractor_->embed(image, faces.front(), aligner_);
        const EnrollmentUpdate update = enrollment_session_.add_sample(sample, now);
        if (update.status == EnrollmentStatus::Completed && update.embedding.has_value()) {
            database_.upsert_person(update.name, *update.embedding);
            database_.save();
            set_status_message("Enrolled " + update.name);
        } else if (update.status == EnrollmentStatus::Accepted) {
            set_status_message(
                "Enrolling " + update.name + ": " + std::to_string(enrollment_session_.sample_count()) +
                "/" + std::to_string(config_.enrollment_sample_count));
        } else if (update.status == EnrollmentStatus::RejectedInconsistent) {
            set_status_message("Enrolling " + update.name + ": hold steady (inconsistent)");
            std::cerr << "Enrollment sample rejected for " << update.name << ": inconsistent embedding\n";
        } else if (update.status == EnrollmentStatus::TimedOut) {
            set_status_message("Enroll timed out: " + update.name);
            std::cerr << "Enrollment timed out for " << update.name << '\n';
        }
    } catch (const std::exception& error) {
        set_status_message(std::string("Enroll error: ") + error.what());
        std::cerr << "Enrollment failed: " << error.what() << '\n';
    } catch (...) {
        set_status_message("Enroll error: unknown");
        std::cerr << "Enrollment failed: unknown error\n";
    }
}

std::vector<RecognitionResult> AppController::process_frame(const Frame& frame) {
    const auto faces = detector_->detect(frame.image);

    handle_enrollment(frame.image, faces, Clock::now());

    const auto db_snapshot = database_.snapshot();
    return recognize_frame_faces(frame, faces, aligner_, *extractor_, db_snapshot, recognizer_);
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
        [this] {
            if (web_server_) {
                web_server_->stop();
            }
        },
        [&camera] { camera.stop(); });
    recognition_running_ = true;
    recognition_thread_ = std::thread(&AppController::recognition_loop, this);
    command_loop_.start();

    try {
        web_server_ = std::make_unique<RuntimeWebServer>(
            [this] { return web_status_snapshot(); },
            [this](const std::string& body) { return web_commands_.push_json(body); },
            [this] { return web_stream_frame(); });
        web_server_->start();
        std::cout << "Web UI: " << web_server_->url() << '\n';
        set_status_message("Web UI: " + web_server_->url());
    } catch (const std::exception& error) {
        std::cerr << "Web UI failed to start: " << error.what() << '\n';
        set_status_message(std::string("Web UI failed: ") + error.what());
    }

    bool running = true;
    auto last_tick = Clock::now();
    double fps = 0.0;

    while (running) {
        apply_commands(running);

        const CameraStatus camera_status = camera.status();
        std::string status = status_message();
        if (!camera_status.opened && !camera_status.last_error.empty()) {
            status = "Camera: " + camera_status.last_error;
        } else {
            const auto recognition_status = recognition_results_.status();
            if (!recognition_status.last_error.empty()) {
                status = "Recognition: " + recognition_status.last_error;
            }
        }

        const auto frame = frame_buffer_.latest();
        if (frame.has_value()) {
            const auto now = Clock::now();
            const double seconds = std::chrono::duration<double>(now - last_tick).count();
            if (seconds > 0.0) {
                fps = 1.0 / seconds;
            }
            fps_ = fps;
            last_tick = now;

            preview.show(
                frame->image,
                recognition_results_.latest_results(),
                database_.snapshot(),
                fps,
                status);
        } else if (!camera_status.opened) {
            // No frames yet: show a placeholder so a failed/opening camera is visible
            // instead of a blank window (all camera errors otherwise only reach stderr).
            cv::Mat placeholder(
                config_.frame_height > 0 ? config_.frame_height : 720,
                config_.frame_width > 0 ? config_.frame_width : 1280,
                CV_8UC3,
                cv::Scalar(24, 24, 24));
            preview.show(
                placeholder,
                {},
                database_.snapshot(),
                fps,
                status.empty() ? ("Waiting for camera index " + std::to_string(config_.camera_index)) : status);
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
    if (web_server_) {
        web_server_->stop();
    }
    camera.stop();
    cleanup.dismiss();
    return 0;
}

}  // namespace ccamera
