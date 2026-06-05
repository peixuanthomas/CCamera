# C++ Camera Face Recognition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a C++ camera face recognition program that captures live video, detects faces, draws face rectangles and red keypoint markers, extracts embeddings, recognizes enrolled identities, and supports live camera enrollment.

**Architecture:** The program is split into small modules: camera capture publishes fresh frames, inference adapters run detector and recognizer models, face modules handle detection output, alignment, database storage, and matching, and UI code draws the preview overlay. The first implementation uses CPU ONNX Runtime through an `InferenceEngine` interface and keeps the rest of the application independent from ONNX Runtime session details.

**Tech Stack:** C++20, CMake, OpenCV, ONNX Runtime CPU, nlohmann/json, standard-library threads, simple executable tests using `assert`.

---

## Implementation Notes

The repository currently contains only design documentation and git history. This plan starts from an empty C++ project.

The first implementation expects the detector ONNX model to expose one output tensor in this normalized row format after model-side or export-side postprocessing:

```text
[x1, y1, x2, y2, score, left_eye_x, left_eye_y, right_eye_x, right_eye_y, nose_x, nose_y, mouth_left_x, mouth_left_y, mouth_right_x, mouth_right_y]
```

Coordinates are pixel coordinates in the original frame after detector preprocessing maps them back. This explicit shape lets the first version draw both face boxes and red keypoint markers without adding a second landmark model.

The recognizer ONNX model is expected to output one embedding vector per aligned face. The implementation L2-normalizes embeddings before database storage and comparison.

## File Structure

Create these files:

```text
CMakeLists.txt
cmake/FindONNXRuntime.cmake
README.md
config/app.example.json
data/.gitkeep
models/README.md
src/main.cpp
src/app/AppController.hpp
src/app/AppController.cpp
src/app/CommandLoop.hpp
src/app/CommandLoop.cpp
src/camera/CameraCapture.hpp
src/camera/CameraCapture.cpp
src/camera/FrameBuffer.hpp
src/camera/FrameBuffer.cpp
src/core/Types.hpp
src/core/Math.hpp
src/core/Math.cpp
src/face/FaceAligner.hpp
src/face/FaceAligner.cpp
src/face/FaceDatabase.hpp
src/face/FaceDatabase.cpp
src/face/FaceDetector.hpp
src/face/FaceDetector.cpp
src/face/FaceFeatureExtractor.hpp
src/face/FaceFeatureExtractor.cpp
src/face/Recognizer.hpp
src/face/Recognizer.cpp
src/inference/InferenceEngine.hpp
src/inference/OnnxRuntimeCpuEngine.hpp
src/inference/OnnxRuntimeCpuEngine.cpp
src/ui/PreviewWindow.hpp
src/ui/PreviewWindow.cpp
tests/test_face_database.cpp
tests/test_frame_buffer.cpp
tests/test_recognizer.cpp
tests/test_visualization_contract.cpp
```

Responsibilities:

- `src/core`: shared structs and math helpers.
- `src/camera`: camera capture and latest-frame synchronization.
- `src/inference`: backend-neutral tensor execution and ONNX Runtime CPU adapter.
- `src/face`: detector decoding, alignment, embedding extraction, database, and matching.
- `src/ui`: OpenCV preview drawing, including face rectangles and red keypoint markers.
- `src/app`: lifecycle, command parsing, enrollment, and worker coordination.
- `tests`: fast tests for logic that does not require a camera or model files.

---

### Task 1: Project Scaffold And Shared Types

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/FindONNXRuntime.cmake`
- Create: `README.md`
- Create: `config/app.example.json`
- Create: `data/.gitkeep`
- Create: `models/README.md`
- Create: `src/core/Types.hpp`
- Create: `src/core/Math.hpp`
- Create: `src/core/Math.cpp`

- [x] **Step 1: Create CMake project**

Create `CMakeLists.txt` with this content:

```cmake
cmake_minimum_required(VERSION 3.20)
project(CCameraFaceRecognition LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CCAMERA_BUILD_TESTS "Build local executable tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio)
find_package(ONNXRuntime REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(ccamera_core
    src/core/Math.cpp
)

target_include_directories(ccamera_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ccamera_core PUBLIC
    opencv_core
)

add_executable(ccamera
    src/main.cpp
)

target_link_libraries(ccamera PRIVATE
    ccamera_core
    opencv_core
    opencv_imgproc
    opencv_highgui
    opencv_videoio
    ONNXRuntime::ONNXRuntime
    nlohmann_json::nlohmann_json
)

if (CCAMERA_BUILD_TESTS)
    enable_testing()
endif()
```

- [x] **Step 2: Create ONNX Runtime CMake finder**

Create `cmake/FindONNXRuntime.cmake` with this content:

```cmake
set(ONNXRUNTIME_ROOT "" CACHE PATH "Path to ONNX Runtime installation root")

find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    HINTS
        "${ONNXRUNTIME_ROOT}/include"
        "$ENV{ONNXRUNTIME_ROOT}/include"
)

find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    HINTS
        "${ONNXRUNTIME_ROOT}/lib"
        "$ENV{ONNXRUNTIME_ROOT}/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ONNXRuntime
    REQUIRED_VARS ONNXRUNTIME_INCLUDE_DIR ONNXRUNTIME_LIBRARY
)

if (ONNXRuntime_FOUND AND NOT TARGET ONNXRuntime::ONNXRuntime)
    add_library(ONNXRuntime::ONNXRuntime UNKNOWN IMPORTED)
    set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
        IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
    )
endif()
```

- [x] **Step 3: Create app configuration example**

Create `config/app.example.json`:

```json
{
  "camera_index": 0,
  "frame_width": 1280,
  "frame_height": 720,
  "detector_model": "models/face_detector.onnx",
  "recognizer_model": "models/face_recognizer.onnx",
  "database_path": "data/face_db.json",
  "inference_backend": "onnxruntime_cpu",
  "detection_interval": 3,
  "detection_threshold": 0.7,
  "recognition_threshold": 0.45,
  "enrollment_consistency_threshold": 0.55,
  "enrollment_sample_count": 15,
  "enrollment_timeout_seconds": 20,
  "feature_worker_count": 2
}
```

- [x] **Step 4: Create shared domain types**

Create `src/core/Types.hpp`:

```cpp
#pragma once

#include <opencv2/core.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ccamera {

using Clock = std::chrono::steady_clock;

struct Frame {
    cv::Mat image;
    std::uint64_t sequence = 0;
    Clock::time_point captured_at{};
};

struct FaceKeypoints {
    std::optional<cv::Point2f> left_eye;
    std::optional<cv::Point2f> right_eye;
    std::optional<cv::Point2f> nose;
    std::optional<cv::Point2f> mouth_left;
    std::optional<cv::Point2f> mouth_right;
};

struct FaceDetection {
    cv::Rect2f box;
    float score = 0.0f;
    FaceKeypoints keypoints;
};

struct FaceEmbedding {
    std::vector<float> values;
};

struct RecognitionResult {
    FaceDetection detection;
    std::string label = "Unknown";
    float similarity = 0.0f;
    bool known = false;
};

struct AppConfig {
    int camera_index = 0;
    int frame_width = 1280;
    int frame_height = 720;
    std::string detector_model = "models/face_detector.onnx";
    std::string recognizer_model = "models/face_recognizer.onnx";
    std::string database_path = "data/face_db.json";
    std::string inference_backend = "onnxruntime_cpu";
    int detection_interval = 3;
    float detection_threshold = 0.7f;
    float recognition_threshold = 0.45f;
    float enrollment_consistency_threshold = 0.55f;
    int enrollment_sample_count = 15;
    int enrollment_timeout_seconds = 20;
    int feature_worker_count = 2;
};

}  // namespace ccamera
```

- [x] **Step 5: Create math helper interface**

Create `src/core/Math.hpp`:

```cpp
#pragma once

#include <vector>

namespace ccamera {

float dot_product(const std::vector<float>& a, const std::vector<float>& b);
float l2_norm(const std::vector<float>& values);
std::vector<float> l2_normalized(const std::vector<float>& values);
float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);

}  // namespace ccamera
```

- [x] **Step 6: Create math helper implementation**

Create `src/core/Math.cpp`:

```cpp
#include "core/Math.hpp"

#include <cmath>
#include <stdexcept>

namespace ccamera {

float dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size()) {
        throw std::invalid_argument("dot_product requires equal vector sizes");
    }
    float result = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

float l2_norm(const std::vector<float>& values) {
    float sum = 0.0f;
    for (float value : values) {
        sum += value * value;
    }
    return std::sqrt(sum);
}

std::vector<float> l2_normalized(const std::vector<float>& values) {
    const float norm = l2_norm(values);
    if (norm <= 1.0e-12f) {
        throw std::invalid_argument("cannot normalize a zero-length vector");
    }
    std::vector<float> normalized;
    normalized.reserve(values.size());
    for (float value : values) {
        normalized.push_back(value / norm);
    }
    return normalized;
}

float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    return dot_product(l2_normalized(a), l2_normalized(b));
}

}  // namespace ccamera
```

- [x] **Step 7: Add minimal entry point**

Create `src/main.cpp`:

```cpp
#include <iostream>

int main() {
    std::cout << "CCamera face recognition scaffold is ready.\n";
    return 0;
}
```

- [x] **Step 8: Create model and project documentation**

Create `models/README.md`:

```markdown
# Models

Place ONNX model files here:

- `face_detector.onnx`: detector output rows must be `[x1, y1, x2, y2, score, left_eye_x, left_eye_y, right_eye_x, right_eye_y, nose_x, nose_y, mouth_left_x, mouth_left_y, mouth_right_x, mouth_right_y]`.
- `face_recognizer.onnx`: recognizer output must be one embedding vector per aligned face.

Model files are not committed to the repository.
```

Create `README.md`:

```markdown
# CCamera Face Recognition

C++ camera face recognition prototype using OpenCV for camera/display and ONNX Runtime for CPU model inference.

First-version features:

- Live camera capture.
- Face rectangles and red face keypoint markers in the preview window.
- Embedding-based local identity recognition.
- Live camera enrollment.
- Local embedding database.

Build dependencies:

- CMake 3.20 or newer.
- OpenCV with `core`, `imgproc`, `highgui`, and `videoio`.
- ONNX Runtime CPU.
- nlohmann/json.

Configure with `-DONNXRUNTIME_ROOT=<path-to-onnxruntime>`.
```

Create an empty `data/.gitkeep`.

- [x] **Step 9: Configure project**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
```

Expected:

```text
Configuring done
Generating done
```

If OpenCV, ONNX Runtime, or nlohmann/json are missing, configuration fails with a package-specific message. Install or point CMake at those dependencies before continuing.

Progress note 2026-06-04: Task 1 implementation, spec review, and code-quality review completed. `cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON` now configures and generates successfully with project-local dependencies: OpenCV from `third_party/msys2/mingw64/lib/cmake/opencv4`, nlohmann/json from `third_party/msys2/mingw64/share/cmake/nlohmann_json`, and ONNX Runtime from `third_party/onnxruntime/onnxruntime-win-x64-1.25.1`. `cmake --build build --config Debug` also succeeds. No manual camera/model checks were run.

- [x] **Step 10: Commit scaffold**

Run:

```powershell
git add CMakeLists.txt cmake README.md config data models src
git commit -m "feat: scaffold C++ face recognition project"
```

---

### Task 2: Recognizer Math And Face Database

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/face/Recognizer.hpp`
- Create: `src/face/Recognizer.cpp`
- Create: `src/face/FaceDatabase.hpp`
- Create: `src/face/FaceDatabase.cpp`
- Create: `tests/test_recognizer.cpp`
- Create: `tests/test_face_database.cpp`

- [x] **Step 1: Add failing recognizer test**

Create `tests/test_recognizer.cpp`:

```cpp
#include "face/Recognizer.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    ccamera::FaceDatabaseSnapshot db;
    db.embedding_dimension = 3;
    db.people.push_back({"person-1", "Alice", {{1.0f, 0.0f, 0.0f}}});
    db.people.push_back({"person-2", "Bob", {{0.0f, 1.0f, 0.0f}}});

    ccamera::Recognizer recognizer(0.8f);

    const auto known = recognizer.match({{0.98f, 0.02f, 0.0f}}, db);
    assert(known.known);
    assert(known.label == "Alice");
    assert(known.similarity > 0.9f);

    const auto unknown = recognizer.match({{0.0f, 0.0f, 1.0f}}, db);
    assert(!unknown.known);
    assert(unknown.label == "Unknown");

    return 0;
}
```

- [x] **Step 2: Add failing database test**

Create `tests/test_face_database.cpp`:

```cpp
#include "face/FaceDatabase.hpp"

#include <cassert>
#include <filesystem>
#include <vector>

int main() {
    const auto path = std::filesystem::temp_directory_path() / "ccamera_face_db_test.json";
    std::filesystem::remove(path);

    ccamera::FaceDatabase db(path.string());
    db.load_or_create();
    db.upsert_person("Alice", {{1.0f, 0.0f, 0.0f}});
    db.upsert_person("Alice", {{0.9f, 0.1f, 0.0f}});
    db.save();

    ccamera::FaceDatabase loaded(path.string());
    loaded.load_or_create();
    const auto snapshot = loaded.snapshot();

    assert(snapshot.people.size() == 1);
    assert(snapshot.people[0].name == "Alice");
    assert(snapshot.people[0].templates.size() == 2);
    assert(snapshot.embedding_dimension == 3);

    loaded.delete_person("Alice");
    assert(loaded.snapshot().people.empty());

    std::filesystem::remove(path);
    return 0;
}
```

- [x] **Step 3: Register tests before implementation**

Modify `CMakeLists.txt` by adding these blocks after `add_library(ccamera_core ...)` source list is expanded:

```cmake
target_sources(ccamera_core PRIVATE
    src/face/FaceDatabase.cpp
    src/face/Recognizer.cpp
)

target_link_libraries(ccamera_core PUBLIC
    nlohmann_json::nlohmann_json
)
```

Add this inside the existing `if (CCAMERA_BUILD_TESTS)` block:

```cmake
add_executable(test_recognizer tests/test_recognizer.cpp)
target_link_libraries(test_recognizer PRIVATE ccamera_core)
add_test(NAME test_recognizer COMMAND test_recognizer)

add_executable(test_face_database tests/test_face_database.cpp)
target_link_libraries(test_face_database PRIVATE ccamera_core)
add_test(NAME test_face_database COMMAND test_face_database)
```

- [x] **Step 4: Run tests and verify compile fails**

Run:

```powershell
cmake --build build --config Debug
```

Expected:

```text
Cannot open include file: 'face/Recognizer.hpp'
```

- [x] **Step 5: Implement face database interface**

Create `src/face/FaceDatabase.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace ccamera {

struct PersonRecord {
    std::string id;
    std::string name;
    std::vector<std::vector<float>> templates;
};

struct FaceDatabaseSnapshot {
    int embedding_dimension = 0;
    std::vector<PersonRecord> people;
};

class FaceDatabase {
public:
    explicit FaceDatabase(std::string path);

    void load_or_create();
    void save() const;
    void upsert_person(const std::string& name, const FaceEmbedding& embedding);
    bool delete_person(const std::string& name);
    FaceDatabaseSnapshot snapshot() const;

private:
    std::string path_;
    mutable std::mutex mutex_;
    FaceDatabaseSnapshot data_;
};

}  // namespace ccamera
```

- [x] **Step 6: Implement face database storage**

Create `src/face/FaceDatabase.cpp`:

```cpp
#include "face/FaceDatabase.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ccamera {
namespace {

std::string make_id(const std::string& name) {
    std::string id;
    id.reserve(name.size());
    for (char ch : name) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            id.push_back(ch);
        } else {
            id.push_back('_');
        }
    }
    return id.empty() ? "person" : id;
}

}  // namespace

FaceDatabase::FaceDatabase(std::string path) : path_(std::move(path)) {}

void FaceDatabase::load_or_create() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = {};
    if (!std::filesystem::exists(path_)) {
        return;
    }

    std::ifstream in(path_);
    if (!in) {
        throw std::runtime_error("failed to open face database for reading: " + path_);
    }

    nlohmann::json json;
    in >> json;
    data_.embedding_dimension = json.value("embedding_dimension", 0);

    for (const auto& person_json : json.value("people", nlohmann::json::array())) {
        PersonRecord record;
        record.id = person_json.at("id").get<std::string>();
        record.name = person_json.at("name").get<std::string>();
        record.templates = person_json.at("templates").get<std::vector<std::vector<float>>>();
        data_.people.push_back(std::move(record));
    }
}

void FaceDatabase::save() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    nlohmann::json json;
    json["embedding_dimension"] = data_.embedding_dimension;
    json["people"] = nlohmann::json::array();
    for (const auto& person : data_.people) {
        json["people"].push_back({
            {"id", person.id},
            {"name", person.name},
            {"templates", person.templates},
        });
    }

    std::ofstream out(path_);
    if (!out) {
        throw std::runtime_error("failed to open face database for writing: " + path_);
    }
    out << json.dump(2) << '\n';
}

void FaceDatabase::upsert_person(const std::string& name, const FaceEmbedding& embedding) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (embedding.values.empty()) {
        throw std::invalid_argument("cannot store empty face embedding");
    }
    if (data_.embedding_dimension == 0) {
        data_.embedding_dimension = static_cast<int>(embedding.values.size());
    }
    if (data_.embedding_dimension != static_cast<int>(embedding.values.size())) {
        throw std::invalid_argument("embedding dimension does not match face database");
    }

    for (auto& person : data_.people) {
        if (person.name == name) {
            person.templates.push_back(embedding.values);
            return;
        }
    }

    data_.people.push_back({make_id(name), name, {embedding.values}});
}

bool FaceDatabase::delete_person(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto old_size = data_.people.size();
    data_.people.erase(
        std::remove_if(data_.people.begin(), data_.people.end(),
                       [&](const PersonRecord& person) { return person.name == name; }),
        data_.people.end());
    return data_.people.size() != old_size;
}

FaceDatabaseSnapshot FaceDatabase::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

}  // namespace ccamera
```

- [x] **Step 7: Implement recognizer**

Create `src/face/Recognizer.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"
#include "face/FaceDatabase.hpp"

namespace ccamera {

struct MatchResult {
    std::string label = "Unknown";
    float similarity = 0.0f;
    bool known = false;
};

class Recognizer {
public:
    explicit Recognizer(float threshold);
    MatchResult match(const FaceEmbedding& embedding, const FaceDatabaseSnapshot& database) const;

private:
    float threshold_;
};

}  // namespace ccamera
```

Create `src/face/Recognizer.cpp`:

```cpp
#include "face/Recognizer.hpp"

#include "core/Math.hpp"

#include <limits>

namespace ccamera {

Recognizer::Recognizer(float threshold) : threshold_(threshold) {}

MatchResult Recognizer::match(const FaceEmbedding& embedding, const FaceDatabaseSnapshot& database) const {
    MatchResult best;
    best.similarity = -std::numeric_limits<float>::infinity();

    for (const auto& person : database.people) {
        for (const auto& templ : person.templates) {
            const float score = cosine_similarity(embedding.values, templ);
            if (score > best.similarity) {
                best.similarity = score;
                best.label = person.name;
            }
        }
    }

    best.known = best.similarity >= threshold_;
    if (!best.known) {
        best.label = "Unknown";
    }
    if (database.people.empty()) {
        best.similarity = 0.0f;
    }
    return best;
}

}  // namespace ccamera
```

- [x] **Step 8: Run tests**

Run:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected:

```text
100% tests passed
```

Progress note 2026-06-04: Task 2 implementation, spec review, and code-quality review completed. `cmake --build build --config Debug` built `ccamera_core`, `ccamera`, `test_recognizer`, and `test_face_database`; `ctest --test-dir build -C Debug --output-on-failure` reported 2/2 tests passed. No manual camera/model checks were run.

- [x] **Step 9: Commit recognizer and database**

Run:

```powershell
git add CMakeLists.txt src/face tests/test_recognizer.cpp tests/test_face_database.cpp
git commit -m "feat: add face database and recognizer"
```

---

### Task 3: Latest Frame Buffer

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/camera/FrameBuffer.hpp`
- Create: `src/camera/FrameBuffer.cpp`
- Create: `tests/test_frame_buffer.cpp`

- [x] **Step 1: Add failing frame buffer test**

Create `tests/test_frame_buffer.cpp`:

```cpp
#include "camera/FrameBuffer.hpp"

#include <cassert>
#include <opencv2/core.hpp>

int main() {
    ccamera::FrameBuffer buffer;
    assert(!buffer.latest().has_value());

    ccamera::Frame first;
    first.sequence = 1;
    first.image = cv::Mat::zeros(2, 2, CV_8UC3);
    buffer.publish(first);

    ccamera::Frame second;
    second.sequence = 2;
    second.image = cv::Mat::ones(2, 2, CV_8UC3);
    buffer.publish(second);

    const auto latest = buffer.latest();
    assert(latest.has_value());
    assert(latest->sequence == 2);
    assert(latest->image.rows == 2);
    assert(latest->image.cols == 2);

    return 0;
}
```

- [x] **Step 2: Register frame buffer code and test**

Modify `CMakeLists.txt`:

```cmake
target_sources(ccamera_core PRIVATE
    src/camera/FrameBuffer.cpp
)

add_executable(test_frame_buffer tests/test_frame_buffer.cpp)
target_link_libraries(test_frame_buffer PRIVATE ccamera_core)
add_test(NAME test_frame_buffer COMMAND test_frame_buffer)
```

- [x] **Step 3: Run build and verify missing header failure**

Run:

```powershell
cmake --build build --config Debug
```

Expected:

```text
Cannot open include file: 'camera/FrameBuffer.hpp'
```

- [x] **Step 4: Implement frame buffer**

Create `src/camera/FrameBuffer.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"

#include <mutex>
#include <optional>

namespace ccamera {

class FrameBuffer {
public:
    void publish(const Frame& frame);
    std::optional<Frame> latest() const;

private:
    mutable std::mutex mutex_;
    std::optional<Frame> latest_;
};

}  // namespace ccamera
```

Create `src/camera/FrameBuffer.cpp`:

```cpp
#include "camera/FrameBuffer.hpp"

namespace ccamera {

void FrameBuffer::publish(const Frame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = frame;
    latest_->image = frame.image.clone();
}

std::optional<Frame> FrameBuffer::latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!latest_.has_value()) {
        return std::nullopt;
    }
    Frame copy = *latest_;
    copy.image = latest_->image.clone();
    return copy;
}

}  // namespace ccamera
```

- [x] **Step 5: Run tests**

Run:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected:

```text
100% tests passed
```

Progress note 2026-06-04: Task 3 implementation, spec review, and code-quality review completed. `cmake --build build --config Debug` built `ccamera_core`, `ccamera`, `test_recognizer`, `test_face_database`, and `test_frame_buffer`; `ctest --test-dir build -C Debug --output-on-failure` reported 3/3 tests passed. No manual camera/model checks were run. `test_frame_buffer` is registered through `cmake -E env` with the project-local MSYS2 runtime bin directory on `PATH` so OpenCV runtime DLLs load during CTest.

- [x] **Step 6: Commit frame buffer**

Run:

```powershell
git add CMakeLists.txt src/camera tests/test_frame_buffer.cpp
git commit -m "feat: add latest frame buffer"
```

---

### Task 4: Inference Interface, Detector Decoding, And Overlay Contract

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/inference/InferenceEngine.hpp`
- Create: `src/face/FaceDetector.hpp`
- Create: `src/face/FaceDetector.cpp`
- Create: `src/ui/PreviewWindow.hpp`
- Create: `src/ui/PreviewWindow.cpp`
- Create: `src/ui/PreviewOverlay.cpp`
- Create: `tests/test_visualization_contract.cpp`

- [x] **Step 1: Add failing visualization contract test**

Create `tests/test_visualization_contract.cpp`:

```cpp
#include "face/FaceDetector.hpp"
#include "inference/InferenceEngine.hpp"
#include "ui/PreviewWindow.hpp"

#include <cassert>
#include <opencv2/core.hpp>

namespace {

class FakeDetectorEngine final : public ccamera::InferenceEngine {
public:
    ccamera::TensorMap run(const ccamera::TensorMap&) override {
        ccamera::Tensor detections;
        detections.shape = {1, 15};
        detections.values = {
            10.0f, 20.0f, 110.0f, 120.0f, 0.91f,
            35.0f, 50.0f,
            85.0f, 50.0f,
            60.0f, 75.0f,
            45.0f, 100.0f,
            75.0f, 100.0f
        };
        return {{"detections", detections}};
    }
};

}  // namespace

int main() {
    FakeDetectorEngine engine;
    ccamera::FaceDetector detector(engine, 0.7f);
    cv::Mat frame = cv::Mat::zeros(160, 160, CV_8UC3);

    const auto faces = detector.detect(frame);
    assert(faces.size() == 1);
    assert(faces[0].score > 0.9f);
    assert(faces[0].keypoints.left_eye.has_value());
    assert(faces[0].keypoints.right_eye.has_value());
    assert(faces[0].keypoints.nose.has_value());
    assert(faces[0].keypoints.mouth_left.has_value());
    assert(faces[0].keypoints.mouth_right.has_value());

    cv::Mat overlay = frame.clone();
    ccamera::PreviewWindow::draw_overlay(overlay, {{faces[0], "Alice", 0.93f, true}});

    const cv::Vec3b left_eye_pixel = overlay.at<cv::Vec3b>(50, 35);
    assert(left_eye_pixel[2] > 180);
    assert(left_eye_pixel[1] < 80);
    assert(left_eye_pixel[0] < 80);

    return 0;
}
```

- [x] **Step 2: Register detector and preview code**

Modify `CMakeLists.txt`:

```cmake
target_sources(ccamera_core PRIVATE
    src/face/FaceDetector.cpp
    src/ui/PreviewWindow.cpp
)

target_link_libraries(ccamera_core PUBLIC
    opencv_imgproc
    opencv_highgui
)

add_executable(test_visualization_contract tests/test_visualization_contract.cpp)
target_link_libraries(test_visualization_contract PRIVATE ccamera_core)
add_test(NAME test_visualization_contract COMMAND test_visualization_contract)
```

- [x] **Step 3: Run build and verify missing inference header**

Run:

```powershell
cmake --build build --config Debug
```

Expected:

```text
Cannot open include file: 'inference/InferenceEngine.hpp'
```

- [x] **Step 4: Implement inference interface**

Create `src/inference/InferenceEngine.hpp`:

```cpp
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ccamera {

struct Tensor {
    std::vector<int64_t> shape;
    std::vector<float> values;
};

using TensorMap = std::unordered_map<std::string, Tensor>;

class InferenceEngine {
public:
    virtual ~InferenceEngine() = default;
    virtual TensorMap run(const TensorMap& inputs) = 0;
};

}  // namespace ccamera
```

- [x] **Step 5: Implement face detector decoding**

Create `src/face/FaceDetector.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>
#include <vector>

namespace ccamera {

class FaceDetector {
public:
    FaceDetector(InferenceEngine& engine, float threshold);
    std::vector<FaceDetection> detect(const cv::Mat& frame);

private:
    InferenceEngine& engine_;
    float threshold_;
};

}  // namespace ccamera
```

Create `src/face/FaceDetector.cpp`:

```cpp
#include "face/FaceDetector.hpp"

#include <algorithm>
#include <stdexcept>

namespace ccamera {

FaceDetector::FaceDetector(InferenceEngine& engine, float threshold)
    : engine_(engine), threshold_(threshold) {}

std::vector<FaceDetection> FaceDetector::detect(const cv::Mat& frame) {
    Tensor input;
    input.shape = {1, frame.channels(), frame.rows, frame.cols};
    input.values.resize(static_cast<std::size_t>(frame.total() * frame.channels()));

    const auto outputs = engine_.run({{"image", input}});
    const auto found = outputs.find("detections");
    if (found == outputs.end()) {
        throw std::runtime_error("detector output 'detections' is missing");
    }

    const Tensor& tensor = found->second;
    if (tensor.shape.size() != 2 || tensor.shape[1] != 15) {
        throw std::runtime_error("detector output must have shape [N, 15]");
    }

    std::vector<FaceDetection> faces;
    const int64_t rows = tensor.shape[0];
    for (int64_t row = 0; row < rows; ++row) {
        const auto base = static_cast<std::size_t>(row * 15);
        const float score = tensor.values[base + 4];
        if (score < threshold_) {
            continue;
        }

        FaceDetection face;
        const float x1 = tensor.values[base + 0];
        const float y1 = tensor.values[base + 1];
        const float x2 = tensor.values[base + 2];
        const float y2 = tensor.values[base + 3];
        face.box = cv::Rect2f(cv::Point2f(x1, y1), cv::Point2f(x2, y2));
        face.score = score;
        face.keypoints.left_eye = cv::Point2f(tensor.values[base + 5], tensor.values[base + 6]);
        face.keypoints.right_eye = cv::Point2f(tensor.values[base + 7], tensor.values[base + 8]);
        face.keypoints.nose = cv::Point2f(tensor.values[base + 9], tensor.values[base + 10]);
        face.keypoints.mouth_left = cv::Point2f(tensor.values[base + 11], tensor.values[base + 12]);
        face.keypoints.mouth_right = cv::Point2f(tensor.values[base + 13], tensor.values[base + 14]);
        faces.push_back(face);
    }

    return faces;
}

}  // namespace ccamera
```

- [x] **Step 6: Implement overlay drawing**

Create `src/ui/PreviewWindow.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace ccamera {

class PreviewWindow {
public:
    explicit PreviewWindow(std::string name);
    void show(const cv::Mat& frame, const std::vector<RecognitionResult>& results, double fps);
    static void draw_overlay(cv::Mat& frame, const std::vector<RecognitionResult>& results);

private:
    std::string name_;
};

}  // namespace ccamera
```

Create `src/ui/PreviewWindow.cpp`:

```cpp
#include "ui/PreviewWindow.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <array>
#include <sstream>
#include <utility>

namespace ccamera {
namespace {

void draw_keypoint(cv::Mat& frame, const std::optional<cv::Point2f>& point) {
    if (!point.has_value()) {
        return;
    }
    cv::circle(frame, *point, 3, cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_AA);
}

}  // namespace

PreviewWindow::PreviewWindow(std::string name) : name_(std::move(name)) {
    cv::namedWindow(name_, cv::WINDOW_NORMAL);
}

void PreviewWindow::show(const cv::Mat& frame, const std::vector<RecognitionResult>& results, double fps) {
    cv::Mat output = frame.clone();
    draw_overlay(output, results);

    std::ostringstream text;
    text << "FPS: " << static_cast<int>(fps);
    cv::putText(output, text.str(), {12, 28}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);

    cv::imshow(name_, output);
}

void PreviewWindow::draw_overlay(cv::Mat& frame, const std::vector<RecognitionResult>& results) {
    for (const auto& result : results) {
        const auto color = result.known ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 255, 255);
        cv::rectangle(frame, result.detection.box, color, 2, cv::LINE_AA);

        const auto& points = result.detection.keypoints;
        draw_keypoint(frame, points.left_eye);
        draw_keypoint(frame, points.right_eye);
        draw_keypoint(frame, points.nose);
        draw_keypoint(frame, points.mouth_left);
        draw_keypoint(frame, points.mouth_right);

        std::ostringstream label;
        label << result.label << " " << static_cast<int>(result.similarity * 100.0f) << "%";
        const cv::Point origin(
            static_cast<int>(result.detection.box.x),
            std::max(20, static_cast<int>(result.detection.box.y) - 6));
        cv::putText(frame, label.str(), origin, cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2, cv::LINE_AA);
    }
}

}  // namespace ccamera
```

- [x] **Step 7: Run tests**

Run:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected:

```text
100% tests passed
```

Progress note 2026-06-04: Task 4 implementation, spec review, and code-quality review completed. Code-quality fixes populated detector input tensors from `cv::Mat` in NCHW float order, restored direct `cv::namedWindow` / `cv::imshow` for the preview executable, and strengthened `test_visualization_contract` to validate detector input and all five red keypoints. The verification blocker was resolved by moving `PreviewWindow::draw_overlay()` into `src/ui/PreviewOverlay.cpp`, keeping `opencv_highgui` out of the headless test link path while `ccamera` still links highgui for the real preview window. Fresh verification passed with `cmake --build build --config Debug` and `ctest --test-dir build -C Debug --output-on-failure` reporting 4/4 tests passed. No manual camera/model checks were run.

- [x] **Step 8: Commit detector and overlay contract**

Run:

```powershell
git add CMakeLists.txt src/inference src/face/FaceDetector.* src/ui tests/test_visualization_contract.cpp
git commit -m "feat: draw face boxes and keypoints"
```

---

### Task 5: Feature Extraction And Enrollment Aggregation

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/face/FaceAligner.hpp`
- Create: `src/face/FaceAligner.cpp`
- Create: `src/face/FaceFeatureExtractor.hpp`
- Create: `src/face/FaceFeatureExtractor.cpp`

- [x] **Step 1: Implement face aligner**

Create `src/face/FaceAligner.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>

namespace ccamera {

class FaceAligner {
public:
    explicit FaceAligner(cv::Size output_size = {112, 112});
    cv::Mat align(const cv::Mat& frame, const FaceDetection& detection) const;

private:
    cv::Size output_size_;
};

}  // namespace ccamera
```

Create `src/face/FaceAligner.cpp`:

```cpp
#include "face/FaceAligner.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <stdexcept>

namespace ccamera {

FaceAligner::FaceAligner(cv::Size output_size) : output_size_(output_size) {}

cv::Mat FaceAligner::align(const cv::Mat& frame, const FaceDetection& detection) const {
    const cv::Rect image_bounds(0, 0, frame.cols, frame.rows);
    cv::Rect crop = detection.box;
    const int margin_x = static_cast<int>(crop.width * 0.15f);
    const int margin_y = static_cast<int>(crop.height * 0.15f);
    crop.x -= margin_x;
    crop.y -= margin_y;
    crop.width += margin_x * 2;
    crop.height += margin_y * 2;
    crop &= image_bounds;

    if (crop.width < 20 || crop.height < 20) {
        throw std::runtime_error("face crop is too small");
    }

    cv::Mat resized;
    cv::resize(frame(crop), resized, output_size_);
    return resized;
}

}  // namespace ccamera
```

- [x] **Step 2: Implement feature extractor**

Create `src/face/FaceFeatureExtractor.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"
#include "inference/InferenceEngine.hpp"

#include <opencv2/core.hpp>

namespace ccamera {

class FaceFeatureExtractor {
public:
    explicit FaceFeatureExtractor(InferenceEngine& engine);
    FaceEmbedding extract(const cv::Mat& aligned_face);

private:
    InferenceEngine& engine_;
};

FaceEmbedding average_embeddings(const std::vector<FaceEmbedding>& embeddings);

}  // namespace ccamera
```

Create `src/face/FaceFeatureExtractor.cpp`:

```cpp
#include "face/FaceFeatureExtractor.hpp"

#include "core/Math.hpp"

#include <stdexcept>

namespace ccamera {

FaceFeatureExtractor::FaceFeatureExtractor(InferenceEngine& engine) : engine_(engine) {}

FaceEmbedding FaceFeatureExtractor::extract(const cv::Mat& aligned_face) {
    Tensor input;
    input.shape = {1, aligned_face.channels(), aligned_face.rows, aligned_face.cols};
    input.values.resize(static_cast<std::size_t>(aligned_face.total() * aligned_face.channels()));

    const auto outputs = engine_.run({{"face", input}});
    const auto found = outputs.find("embedding");
    if (found == outputs.end()) {
        throw std::runtime_error("recognizer output 'embedding' is missing");
    }

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
```

- [x] **Step 3: Register aligner and feature extractor**

Modify `CMakeLists.txt`:

```cmake
target_sources(ccamera_core PRIVATE
    src/face/FaceAligner.cpp
    src/face/FaceFeatureExtractor.cpp
)
```

- [x] **Step 4: Run tests**

Run:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected:

```text
100% tests passed
```

Progress note 2026-06-04: Task 5 implementation, spec review, and code-quality review completed. `FaceFeatureExtractor` validates recognizer embedding output shape/value count before normalization, accepts `[N]` and `[1, N]`, and packs aligned faces in NCHW float order. `FaceAligner` rejects invalid output sizes, empty frames, and too-small crops. Added `test_face_feature_extraction` for aligner behavior, tensor layout, malformed recognizer outputs, L2 normalization, and aggregation mismatch handling. Fresh verification passed with `cmake --build build --config Debug` and `ctest --test-dir build -C Debug --output-on-failure` reporting 5/5 tests passed. No manual camera/model checks were run.

- [x] **Step 5: Commit feature extraction utilities**

Run:

```powershell
git add CMakeLists.txt src/face/FaceAligner.* src/face/FaceFeatureExtractor.*
git commit -m "feat: add face alignment and embedding extraction"
```

---

### Task 6: ONNX Runtime CPU Engine

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/inference/OnnxRuntimeCpuEngine.hpp`
- Create: `src/inference/OnnxRuntimeCpuEngine.cpp`

- [x] **Step 1: Implement ONNX Runtime CPU engine interface**

Create `src/inference/OnnxRuntimeCpuEngine.hpp`:

```cpp
#pragma once

#include "inference/InferenceEngine.hpp"

#include <onnxruntime_cxx_api.h>

#include <memory>
#include <string>
#include <vector>

namespace ccamera {

class OnnxRuntimeCpuEngine final : public InferenceEngine {
public:
    explicit OnnxRuntimeCpuEngine(const std::string& model_path);
    TensorMap run(const TensorMap& inputs) override;

private:
    Ort::Env env_;
    Ort::SessionOptions options_;
    Ort::Session session_;
    Ort::AllocatorWithDefaultOptions allocator_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
};

}  // namespace ccamera
```

- [x] **Step 2: Implement ONNX Runtime CPU engine**

Create `src/inference/OnnxRuntimeCpuEngine.cpp`:

```cpp
#include "inference/OnnxRuntimeCpuEngine.hpp"

#include <stdexcept>
#include <utility>

namespace ccamera {

OnnxRuntimeCpuEngine::OnnxRuntimeCpuEngine(const std::string& model_path)
    : env_(ORT_LOGGING_LEVEL_WARNING, "ccamera"),
      options_(),
      session_(nullptr) {
    options_.SetIntraOpNumThreads(1);
    options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    session_ = Ort::Session(env_, model_path.c_str(), options_);

    const std::size_t input_count = session_.GetInputCount();
    for (std::size_t i = 0; i < input_count; ++i) {
        auto name = session_.GetInputNameAllocated(i, allocator_);
        input_names_.push_back(name.get());
    }

    const std::size_t output_count = session_.GetOutputCount();
    for (std::size_t i = 0; i < output_count; ++i) {
        auto name = session_.GetOutputNameAllocated(i, allocator_);
        output_names_.push_back(name.get());
    }
}

TensorMap OnnxRuntimeCpuEngine::run(const TensorMap& inputs) {
    std::vector<const char*> input_name_ptrs;
    std::vector<Ort::Value> input_values;
    input_name_ptrs.reserve(input_names_.size());
    input_values.reserve(input_names_.size());

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    for (const auto& name : input_names_) {
        const auto found = inputs.find(name);
        if (found == inputs.end()) {
            throw std::runtime_error("missing input tensor: " + name);
        }
        const Tensor& tensor = found->second;
        input_name_ptrs.push_back(name.c_str());
        input_values.push_back(Ort::Value::CreateTensor<float>(
            memory_info,
            const_cast<float*>(tensor.values.data()),
            tensor.values.size(),
            tensor.shape.data(),
            tensor.shape.size()));
    }

    std::vector<const char*> output_name_ptrs;
    output_name_ptrs.reserve(output_names_.size());
    for (const auto& name : output_names_) {
        output_name_ptrs.push_back(name.c_str());
    }

    auto ort_outputs = session_.Run(
        Ort::RunOptions{nullptr},
        input_name_ptrs.data(),
        input_values.data(),
        input_values.size(),
        output_name_ptrs.data(),
        output_name_ptrs.size());

    TensorMap outputs;
    for (std::size_t i = 0; i < ort_outputs.size(); ++i) {
        auto& ort_value = ort_outputs[i];
        auto type_info = ort_value.GetTensorTypeAndShapeInfo();
        Tensor tensor;
        tensor.shape = type_info.GetShape();
        const auto element_count = type_info.GetElementCount();
        const float* data = ort_value.GetTensorData<float>();
        tensor.values.assign(data, data + element_count);
        outputs.emplace(output_names_[i], std::move(tensor));
    }
    return outputs;
}

}  // namespace ccamera
```

- [x] **Step 3: Register ONNX Runtime engine**

Modify `CMakeLists.txt`:

```cmake
target_sources(ccamera_core PRIVATE
    src/inference/OnnxRuntimeCpuEngine.cpp
)

target_link_libraries(ccamera_core PUBLIC
    ONNXRuntime::ONNXRuntime
)
```

- [x] **Step 4: Build**

Run:

```powershell
cmake --build build --config Debug
```

Expected:

```text
Build succeeded
```

Progress note 2026-06-04: Task 6 implementation, spec review, and code-quality review completed. `OnnxRuntimeCpuEngine` now owns a CPU ONNX Runtime session, records model input/output names, requires inputs by exact model name, runs float CPU tensors through ONNX Runtime, and returns outputs as `TensorMap`. The engine is registered in `ccamera_core` and links `ONNXRuntime::ONNXRuntime` publicly because the public header includes ONNX Runtime types. Fresh verification passed with `cmake --build build --config Debug`. No manual camera/model checks were run, and no ONNX model execution results were fabricated.

- [x] **Step 5: Commit ONNX Runtime engine**

Run:

```powershell
git add CMakeLists.txt src/inference/OnnxRuntimeCpuEngine.*
git commit -m "feat: add ONNX Runtime CPU inference engine"
```

---

### Task 7: Camera Capture And App Controller

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/camera/CameraCapture.hpp`
- Create: `src/camera/CameraCapture.cpp`
- Create: `src/app/AppController.hpp`
- Create: `src/app/AppController.cpp`
- Modify: `src/main.cpp`

- [x] **Step 1: Implement camera capture**

Create `src/camera/CameraCapture.hpp`:

```cpp
#pragma once

#include "camera/FrameBuffer.hpp"
#include "core/Types.hpp"

#include <atomic>
#include <thread>

namespace ccamera {

class CameraCapture {
public:
    CameraCapture(AppConfig config, FrameBuffer& buffer);
    ~CameraCapture();

    void start();
    void stop();

private:
    void run();

    AppConfig config_;
    FrameBuffer& buffer_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}  // namespace ccamera
```

Create `src/camera/CameraCapture.cpp`:

```cpp
#include "camera/CameraCapture.hpp"

#include <opencv2/videoio.hpp>

#include <stdexcept>
#include <utility>

namespace ccamera {

CameraCapture::CameraCapture(AppConfig config, FrameBuffer& buffer)
    : config_(std::move(config)), buffer_(buffer) {}

CameraCapture::~CameraCapture() {
    stop();
}

void CameraCapture::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&CameraCapture::run, this);
}

void CameraCapture::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void CameraCapture::run() {
    cv::VideoCapture capture(config_.camera_index);
    if (!capture.isOpened()) {
        running_ = false;
        return;
    }

    capture.set(cv::CAP_PROP_FRAME_WIDTH, config_.frame_width);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, config_.frame_height);

    std::uint64_t sequence = 0;
    while (running_) {
        cv::Mat image;
        if (!capture.read(image) || image.empty()) {
            continue;
        }

        Frame frame;
        frame.image = image;
        frame.sequence = ++sequence;
        frame.captured_at = Clock::now();
        buffer_.publish(frame);
    }
}

}  // namespace ccamera
```

- [x] **Step 2: Implement app controller vertical slice**

Create `src/app/AppController.hpp`:

```cpp
#pragma once

#include "camera/FrameBuffer.hpp"
#include "core/Types.hpp"

namespace ccamera {

class AppController {
public:
    explicit AppController(AppConfig config);
    int run();

private:
    AppConfig config_;
    FrameBuffer frame_buffer_;
};

}  // namespace ccamera
```

Create `src/app/AppController.cpp`:

```cpp
#include "app/AppController.hpp"

#include "camera/CameraCapture.hpp"
#include "ui/PreviewWindow.hpp"

#include <opencv2/highgui.hpp>

namespace ccamera {

AppController::AppController(AppConfig config) : config_(std::move(config)) {}

int AppController::run() {
    CameraCapture camera(config_, frame_buffer_);
    PreviewWindow preview("CCamera");

    camera.start();

    bool running = true;
    while (running) {
        const auto frame = frame_buffer_.latest();
        if (frame.has_value()) {
            preview.show(frame->image, {}, 0.0);
        }

        const int key = cv::waitKey(1);
        if (key == 'q' || key == 'Q' || key == 27) {
            running = false;
        }
    }

    camera.stop();
    return 0;
}

}  // namespace ccamera
```

- [x] **Step 3: Wire main to app controller**

Replace `src/main.cpp`:

```cpp
#include "app/AppController.hpp"

int main() {
    ccamera::AppConfig config;
    ccamera::AppController app(config);
    return app.run();
}
```

- [x] **Step 4: Register app and camera files**

Modify `CMakeLists.txt`:

```cmake
target_sources(ccamera_core PRIVATE
    src/app/AppController.cpp
    src/camera/CameraCapture.cpp
)

target_link_libraries(ccamera_core PUBLIC
    opencv_videoio
)
```

- [ ] **Step 5: Build and manually verify camera preview**

Run:

```powershell
cmake --build build --config Debug
.\build\Debug\ccamera.exe
```

Expected:

```text
An OpenCV window opens and shows the live camera feed.
Press Q or Esc to quit.
```

- [x] **Step 6: Commit camera vertical slice**

Progress note 2026-06-04: Task 7 implementation, spec review, and code-quality review completed. `CameraCapture` now reads frames on a background thread, publishes latest frames through `FrameBuffer`, and `AppController` shows the live preview until `Q`/`Esc`. Fresh automated verification passed with `cmake --build build --config Debug` and `ctest --test-dir build -C Debug --output-on-failure` reporting 5/5 tests passed. Manual camera preview was not run, so Task 7 Step 5 remains unchecked and must be treated as MANUAL/NOT RUN rather than passed.

Run:

```powershell
git add CMakeLists.txt src/main.cpp src/app src/camera/CameraCapture.*
git commit -m "feat: show live camera preview"
```

---

### Task 8: Recognition Pipeline In Preview

**Files:**
- Modify: `src/app/AppController.hpp`
- Modify: `src/app/AppController.cpp`

- [x] **Step 1: Add detector, aligner, extractor, database, and recognizer members**

Modify `src/app/AppController.hpp`:

```cpp
#pragma once

#include "camera/FrameBuffer.hpp"
#include "core/Types.hpp"
#include "face/FaceAligner.hpp"
#include "face/FaceDatabase.hpp"
#include "face/FaceDetector.hpp"
#include "face/FaceFeatureExtractor.hpp"
#include "face/Recognizer.hpp"
#include "inference/OnnxRuntimeCpuEngine.hpp"

#include <memory>
#include <opencv2/core.hpp>
#include <vector>

namespace ccamera {

class AppController {
public:
    explicit AppController(AppConfig config);
    int run();

private:
    std::vector<RecognitionResult> process_frame(const cv::Mat& image);

    AppConfig config_;
    FrameBuffer frame_buffer_;
    OnnxRuntimeCpuEngine detector_engine_;
    OnnxRuntimeCpuEngine recognizer_engine_;
    FaceDetector detector_;
    FaceAligner aligner_;
    FaceFeatureExtractor extractor_;
    FaceDatabase database_;
    Recognizer recognizer_;
};

}  // namespace ccamera
```

- [x] **Step 2: Process frames through recognition**

Modify `src/app/AppController.cpp`:

```cpp
#include "app/AppController.hpp"

#include "camera/CameraCapture.hpp"
#include "ui/PreviewWindow.hpp"

#include <opencv2/highgui.hpp>

#include <chrono>
#include <exception>
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

std::vector<RecognitionResult> AppController::process_frame(const cv::Mat& image) {
    std::vector<RecognitionResult> results;
    const auto faces = detector_.detect(image);
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

    bool running = true;
    auto last_tick = Clock::now();
    double fps = 0.0;

    while (running) {
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

    camera.stop();
    return 0;
}

}  // namespace ccamera
```

- [ ] **Step 3: Build and run with model files**

Run:

```powershell
cmake --build build --config Debug
.\build\Debug\ccamera.exe
```

Expected:

```text
The preview shows detected face rectangles.
Detected landmark positions are drawn as red points.
Known people show their name when database entries exist.
Unknown people show Unknown.
```

- [x] **Step 4: Commit recognition pipeline**

Progress note 2026-06-04: Task 8 implementation, spec review, and code-quality review completed. `AppController` now owns detector and recognizer ONNX Runtime engines, processes preview frames through detection, alignment, embedding extraction, database matching, and renders recognition results with FPS. Fresh automated verification passed with `cmake --build build --config Debug` and `ctest --test-dir build -C Debug --output-on-failure` reporting 5/5 tests passed. Compatible `models/face_detector.onnx` and `models/face_recognizer.onnx` files are absent, so Task 8 Step 3 remains unchecked and the model/camera preview check is MANUAL/BLOCKED rather than passed.

Run:

```powershell
git add src/app/AppController.*
git commit -m "feat: run recognition pipeline in preview"
```

---

### Task 9: Command Loop And Live Enrollment

**Files:**
- Modify: `CMakeLists.txt`
- Create: `src/app/CommandLoop.hpp`
- Create: `src/app/CommandLoop.cpp`
- Modify: `src/app/AppController.hpp`
- Modify: `src/app/AppController.cpp`

- [x] **Step 1: Implement command loop**

Create `src/app/CommandLoop.hpp`:

```cpp
#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace ccamera {

struct CommandState {
    std::optional<std::string> enroll_name;
    std::optional<std::string> delete_name;
    bool list_requested = false;
    bool quit_requested = false;
};

class CommandLoop {
public:
    CommandLoop();
    ~CommandLoop();

    void start();
    void stop();
    CommandState consume();

private:
    void run();

    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mutex_;
    CommandState state_;
};

}  // namespace ccamera
```

Create `src/app/CommandLoop.cpp`:

```cpp
#include "app/CommandLoop.hpp"

#include <iostream>
#include <sstream>

namespace ccamera {

CommandLoop::CommandLoop() = default;

CommandLoop::~CommandLoop() {
    stop();
}

void CommandLoop::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&CommandLoop::run, this);
}

void CommandLoop::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

CommandState CommandLoop::consume() {
    std::lock_guard<std::mutex> lock(mutex_);
    CommandState copy = state_;
    state_ = {};
    return copy;
}

void CommandLoop::run() {
    std::cout << "Commands: enroll <name>, delete <name>, list, quit\n";
    std::string line;
    while (running_ && std::getline(std::cin, line)) {
        std::istringstream stream(line);
        std::string command;
        stream >> command;

        std::lock_guard<std::mutex> lock(mutex_);
        if (command == "enroll") {
            std::string name;
            stream >> name;
            if (!name.empty()) {
                state_.enroll_name = name;
            }
        } else if (command == "delete") {
            std::string name;
            stream >> name;
            if (!name.empty()) {
                state_.delete_name = name;
            }
        } else if (command == "list") {
            state_.list_requested = true;
        } else if (command == "quit") {
            state_.quit_requested = true;
            running_ = false;
        }
    }
}

}  // namespace ccamera
```

- [x] **Step 2: Add enrollment state to controller**

Modify `src/app/AppController.hpp` by adding:

```cpp
#include "app/CommandLoop.hpp"
```

Add private members:

```cpp
CommandLoop command_loop_;
std::optional<std::string> enrollment_name_;
std::vector<FaceEmbedding> enrollment_samples_;
```

Add private methods:

```cpp
void apply_commands(bool& running);
void handle_enrollment(const cv::Mat& image, const std::vector<FaceDetection>& faces);
```

- [x] **Step 3: Implement command handling and enrollment**

Modify `src/app/AppController.cpp`. Add `#include <iostream>` with the existing includes:

```cpp
#include <iostream>
```

Add these methods:

```cpp
void AppController::apply_commands(bool& running) {
    const CommandState commands = command_loop_.consume();
    if (commands.quit_requested) {
        running = false;
    }
    if (commands.enroll_name.has_value()) {
        enrollment_name_ = commands.enroll_name;
        enrollment_samples_.clear();
    }
    if (commands.delete_name.has_value()) {
        database_.delete_person(*commands.delete_name);
        database_.save();
    }
    if (commands.list_requested) {
        const auto snapshot = database_.snapshot();
        for (const auto& person : snapshot.people) {
            std::cout << person.name << " (" << person.templates.size() << " templates)\n";
        }
    }
}

void AppController::handle_enrollment(const cv::Mat& image, const std::vector<FaceDetection>& faces) {
    if (!enrollment_name_.has_value()) {
        return;
    }
    if (faces.size() != 1) {
        return;
    }

    const cv::Mat aligned = aligner_.align(image, faces.front());
    enrollment_samples_.push_back(extractor_.extract(aligned));

    if (static_cast<int>(enrollment_samples_.size()) >= config_.enrollment_sample_count) {
        FaceEmbedding averaged = average_embeddings(enrollment_samples_);
        database_.upsert_person(*enrollment_name_, averaged);
        database_.save();
        enrollment_name_.reset();
        enrollment_samples_.clear();
    }
}
```

In `run()`, start and stop the command loop:

```cpp
command_loop_.start();
```

inside startup, and:

```cpp
command_loop_.stop();
```

before returning.

Replace the current `process_frame` method with this version so enrollment and recognition use the same detections that are displayed:

```cpp
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
```

In the frame loop, call:

```cpp
apply_commands(running);
```

Enrollment is now handled inside `process_frame` immediately after detection, before recognition reads a fresh database snapshot.

- [x] **Step 4: Register command loop**

Modify `CMakeLists.txt`:

```cmake
target_sources(ccamera_core PRIVATE
    src/app/CommandLoop.cpp
)
```

- [ ] **Step 5: Build and manually test commands**

Run:

```powershell
cmake --build build --config Debug
.\build\Debug\ccamera.exe
```

Manual commands:

```text
list
enroll Alice
list
delete Alice
list
quit
```

Expected:

```text
After enroll Alice, keeping one face visible for the configured sample count creates an Alice entry.
After delete Alice, Alice is removed from the database.
quit exits the process.
```

- [x] **Step 6: Commit command loop and enrollment**

Progress note 2026-06-05: Task 9 implementation, spec review, and code-quality review completed. `CommandLoop` now parses `enroll <name>`, `delete <name>`, `list`, and `quit`, preserves queued commands in input order, and uses nonblocking Windows console/pipe handling so shutdown through preview keys can join the command thread. `AppController` applies queued commands, handles live enrollment from exactly one detected face, catches command/enrollment failures without terminating the preview loop, and saves database changes. Fresh automated verification passed with `cmake --build build --config Debug` and `ctest --test-dir build -C Debug --output-on-failure` reporting 6/6 tests passed, including `test_command_loop`. Manual command/camera enrollment testing was not run with hardware and compatible ONNX model files, so Task 9 Step 5 remains unchecked and MANUAL/BLOCKED rather than passed.

Run:

```powershell
git add CMakeLists.txt src/app
git commit -m "feat: add command loop and live enrollment"
```

---

### Task 10: Threaded Processing And Final Verification

**Files:**
- Modify: `src/app/AppController.hpp`
- Modify: `src/app/AppController.cpp`
- Modify: `README.md`

- [x] **Step 1: Add worker ownership to controller**

Modify `src/app/AppController.hpp` with these members:

```cpp
std::atomic<bool> recognition_running_{false};
std::thread recognition_thread_;
std::mutex results_mutex_;
std::vector<RecognitionResult> latest_results_;
std::uint64_t last_processed_sequence_ = 0;
```

Add private methods:

```cpp
void recognition_loop();
std::vector<RecognitionResult> latest_results();
```

- [x] **Step 2: Move recognition to worker loop**

Modify `src/app/AppController.cpp`:

```cpp
void AppController::recognition_loop() {
    while (recognition_running_) {
        const auto frame = frame_buffer_.latest();
        if (!frame.has_value() || frame->sequence == last_processed_sequence_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        last_processed_sequence_ = frame->sequence;
        std::vector<RecognitionResult> results = process_frame(frame->image);

        std::lock_guard<std::mutex> lock(results_mutex_);
        latest_results_ = std::move(results);
    }
}

std::vector<RecognitionResult> AppController::latest_results() {
    std::lock_guard<std::mutex> lock(results_mutex_);
    return latest_results_;
}
```

In `run()`, start recognition after camera startup:

```cpp
recognition_running_ = true;
recognition_thread_ = std::thread(&AppController::recognition_loop, this);
```

On shutdown:

```cpp
recognition_running_ = false;
if (recognition_thread_.joinable()) {
    recognition_thread_.join();
}
```

In preview rendering, use:

```cpp
preview.show(frame->image, latest_results(), fps);
```

- [x] **Step 3: Add required includes**

Add these includes to `src/app/AppController.hpp`:

```cpp
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
```

Add this include to `src/app/AppController.cpp`:

```cpp
#include <thread>
```

- [x] **Step 4: Update README run instructions**

Append to `README.md`:

```markdown
## Running

1. Place compatible detector and recognizer ONNX files in `models/`.
2. Configure dependency paths with CMake.
3. Build the executable.
4. Run `ccamera.exe`.

The preview window draws rectangles around detected faces and red points on detected face keypoints. The command line accepts `enroll <name>`, `delete <name>`, `list`, and `quit`.

Press `Q` or `Esc` in the preview window to close the app.
```

- [x] **Step 5: Run automated verification**

Run:

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 6: Run manual visual verification**

Run:

```powershell
.\build\Debug\ccamera.exe
```

Expected:

```text
The camera preview remains smooth.
Detected faces have rectangles.
Detected left eye, right eye, nose, and mouth corners are red points.
The preview does not freeze while recognition is running.
```

- [x] **Step 7: Commit threaded processing**

Run:

```powershell
git add src/app README.md
git commit -m "feat: run recognition on worker thread"
```

Progress note 2026-06-05: Task 10 automated implementation and review-risk hardening completed. `AppController` now runs recognition on a worker thread, contains detector/recognizer exceptions through `RecognitionResultsStore`, tags recognition results with frame sequences, and only draws overlays for the current frame sequence so stale results clear. `main()` supports `--config`/positional config paths and validates `config/app.example.json`; unknown `inference_backend` values and obvious invalid numeric/path fields fail before app startup. Model contracts are explicit in `src/face/ModelContracts.hpp`, `README.md`, and `models/README.md`; tensor shape/value validation is used before ONNX Runtime inputs and in face pre/postprocessing. `CameraCapture` exposes a testable status/error surface and supports fake camera sources for hardware-free open/read failure tests. Enrollment now enforces `enrollment_timeout_seconds` and rejects inconsistent samples using cosine similarity before storing an averaged embedding. TDD red verification failed first on missing `app/AppConfig.hpp`; final automated verification passed with `cmake --build build --config Debug` and `ctest --test-dir build -C Debug --output-on-failure` reporting 11/11 tests passed. Manual visual camera/model verification was not run, so Task 10 Step 6 remains unchecked and MANUAL/NOT RUN rather than passed.

Code-quality review fix note 2026-06-05: `AppController::run()` now uses `AppRunCleanup` so recognition, command loop, and camera cleanup also runs on exception paths after startup. `CameraCapture` backs off for 5 ms after read failures to avoid tight retry loops. TDD red verification first failed with `test_camera_capture_status` exceeding the immediate-failure read-count limit and `test_app_run_cleanup` failing on the missing cleanup helper; final automated verification passed with `cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON`, `cmake --build build --config Debug`, and `ctest --test-dir build -C Debug --output-on-failure` reporting 12/12 tests passed. Manual visual camera/model verification was not run, so Task 10 Step 6 remains unchecked and MANUAL/NOT RUN rather than passed.

---

### Offline Virtual Dataset Test Support

- [x] **Step 1: Add no-camera virtual dataset automated test**

Progress note 2026-06-05: Added `test_offline_virtual_dataset` with a deterministic test-only virtual face dataset, synthetic OpenCV frame, and fake detector/recognizer inference engines. The test exercises current pipeline components through detector tensor packing/output decoding, face alignment, feature extraction normalization, database snapshot matching, and recognition result assembly without calling `CameraCapture`, opening a camera, constructing `OnnxRuntimeCpuEngine`, or loading real ONNX model files. TDD red verification first failed on missing `support/VirtualFaceDataset.hpp`; final automated verification passed with `cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON`, `cmake --build build --config Debug`, `ctest --test-dir build -C Debug -R test_offline_virtual_dataset --output-on-failure`, and `ctest --test-dir build -C Debug --output-on-failure` reporting 13/13 tests passed. Manual camera/model checks were not run and remain unchecked.

---

## Final Acceptance Checklist

- [x] CMake configure succeeds with OpenCV, ONNX Runtime, and nlohmann/json available.
- [x] `cmake --build build --config Debug` succeeds.
- [x] `ctest --test-dir build -C Debug --output-on-failure` reports all tests passed.
- [ ] Camera preview opens.
- [ ] Face rectangles are drawn around detected faces.
- [ ] Red keypoint markers are drawn on detected face landmarks.
- [ ] `enroll <name>` stores a live camera enrollment.
- [ ] `list` shows enrolled identities.
- [ ] Recognition displays the enrolled name when similarity passes the configured threshold.
- [ ] Unknown faces are labeled `Unknown`.
- [ ] `delete <name>` removes the identity from the local database.
- [ ] `quit`, `Q`, and `Esc` stop the application.
