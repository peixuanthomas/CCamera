# Direct Usable OpenCV Zoo Default Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the current Windows build produce a `ccamera.exe` that can be double-clicked to enter the preview path using committed OpenCV Zoo YuNet/SFace models.

**Architecture:** Keep the current launcher/runtime split and app controller flow. Add an `opencv_zoo` backend that wraps OpenCV Zoo YuNet/SFace behind the same face detection and embedding abstractions used by the rest of the application, while keeping the existing `onnxruntime_cpu` backend available for custom configs.

**Tech Stack:** C++20, CMake, OpenCV core/imgproc/highgui/videoio/objdetect, ONNX Runtime CPU, nlohmann/json, PowerShell download commands, executable CTest tests.

---

## Execution Rules

Before every task and before dispatching every subagent, read:

- `AGENTS.md`
- `docs/discoveries.md`
- The active Superpowers skill file

For each implementation task, the controller must perform the repository-required flow:

1. Dispatch a fresh implementation subagent.
2. Dispatch a spec-compliance review subagent.
3. Dispatch a code-quality review subagent.
4. Fix all review findings.
5. Run the verification commands listed in that task.
6. Update this plan's checkboxes.
7. Update `docs/discoveries.md`.
8. Commit the task.

Do not mark live camera, real face recognition, live enrollment, or GUI interaction as passed unless those manual checks are actually run.

## File Structure

Create:

- `models/opencv_zoo/face_detection_yunet_2023mar.onnx` - committed YuNet detector model.
- `models/opencv_zoo/face_recognition_sface_2021dec.onnx` - committed SFace recognizer model.
- `models/opencv_zoo/LICENSE.yunet.txt` - YuNet MIT license text from OpenCV Zoo.
- `models/opencv_zoo/LICENSE.sface.txt` - SFace Apache 2.0 license text from OpenCV Zoo.
- `models/opencv_zoo/SOURCES.md` - source URLs, model filenames, and license summary.
- `src/face/FaceServices.hpp` - small backend-neutral interfaces for detection and embedding extraction.
- `src/face/OpenCvZooFaceDetector.hpp`
- `src/face/OpenCvZooFaceDetector.cpp`
- `src/face/OpenCvZooFaceFeatureExtractor.hpp`
- `src/face/OpenCvZooFaceFeatureExtractor.cpp`
- `src/app/StartupMode.hpp`
- `src/app/StartupMode.cpp`
- `tests/test_opencv_zoo_adapters.cpp`
- `tests/test_startup_mode.cpp`

Modify:

- `CMakeLists.txt` - link `opencv_objdetect`, build new sources/tests, update startup CTest modes.
- `config/app.example.json` - default to committed OpenCV Zoo model paths and `opencv_zoo` backend.
- `models/README.md` - document committed default models and custom model option.
- `README.md` - update build/run instructions for double-click default use.
- `src/core/Types.hpp` - update default model paths and backend string.
- `src/app/AppConfig.cpp` - allow `opencv_zoo` and preserve `onnxruntime_cpu`.
- `src/app/AppController.hpp`
- `src/app/AppController.cpp` - select backend implementation at construction.
- `src/app/FrameRecognition.hpp`
- `src/app/FrameRecognition.cpp` - depend on backend-neutral embedding interface.
- `src/face/FaceDetector.hpp` - implement backend-neutral detector interface.
- `src/face/FaceFeatureExtractor.hpp` - implement backend-neutral embedding interface.
- `src/main.cpp` - use no-argument default runtime startup instead of startup config UI.
- `cmake/CheckExecutableStartup.cmake` - use missing-model and dummy-model probes that do not depend on the now-valid default config.
- `tests/test_app_config.cpp` - assert default OpenCV Zoo config and committed model presence.
- `docs/discoveries.md` - record task outcomes and manual verification limits.

---

### Task 1: Commit OpenCV Zoo Models And Default Config

**Files:**
- Create: `models/opencv_zoo/face_detection_yunet_2023mar.onnx`
- Create: `models/opencv_zoo/face_recognition_sface_2021dec.onnx`
- Create: `models/opencv_zoo/LICENSE.yunet.txt`
- Create: `models/opencv_zoo/LICENSE.sface.txt`
- Create: `models/opencv_zoo/SOURCES.md`
- Modify: `config/app.example.json`
- Modify: `models/README.md`
- Modify: `src/core/Types.hpp`
- Modify: `src/app/AppConfig.cpp`
- Modify: `tests/test_app_config.cpp`
- Modify: `docs/discoveries.md`

- [x] **Step 1: Add failing default-config assertions**

Edit `tests/test_app_config.cpp`. Replace the initial example assertions with:

```cpp
    const auto example = ccamera::load_app_config("config/app.example.json");
    assert(example.camera_index == 0);
    assert(example.frame_width == 1280);
    assert(example.frame_height == 720);
    assert(example.inference_backend == "opencv_zoo");
    assert(example.detector_model == "models/opencv_zoo/face_detection_yunet_2023mar.onnx");
    assert(example.recognizer_model == "models/opencv_zoo/face_recognition_sface_2021dec.onnx");
    assert(std::filesystem::exists(example.detector_model));
    assert(std::filesystem::exists(example.recognizer_model));
    assert(example.enrollment_consistency_threshold == 0.55f);
```

Add this backend override check after the existing `override_config` assertions:

```cpp
    assert(override_config.inference_backend == "onnxruntime_cpu");
```

Replace the bad backend assertion:

```cpp
    expect_invalid_config(
        [] {
            const auto path = write_config("ccamera_bad_backend.json", R"json({"inference_backend":"mock"})json");
            (void)ccamera::load_app_config(path.string());
        },
        "inference_backend");
```

with:

```cpp
    {
        ccamera::AppConfig config;
        config.inference_backend = "opencv_zoo";
        ccamera::validate_app_config(config);
        config.inference_backend = "onnxruntime_cpu";
        ccamera::validate_app_config(config);
    }

    expect_invalid_config(
        [] {
            const auto path = write_config("ccamera_bad_backend.json", R"json({"inference_backend":"mock"})json");
            (void)ccamera::load_app_config(path.string());
        },
        "inference_backend");
```

- [x] **Step 2: Run the focused test and verify it fails**

Run:

```powershell
cmake --build build --config Debug --target test_app_config
ctest --test-dir build -C Debug -R test_app_config --output-on-failure
```

Expected: `test_app_config` fails because `config/app.example.json` still uses `onnxruntime_cpu` and the OpenCV Zoo model files do not exist.

- [x] **Step 3: Add model files and source/license docs**

Run these commands from the repository root:

```powershell
New-Item -ItemType Directory -Force -Path models\opencv_zoo | Out-Null
Invoke-WebRequest -Uri "https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx" -OutFile "models\opencv_zoo\face_detection_yunet_2023mar.onnx"
Invoke-WebRequest -Uri "https://github.com/opencv/opencv_zoo/raw/main/models/face_recognition_sface/face_recognition_sface_2021dec.onnx" -OutFile "models\opencv_zoo\face_recognition_sface_2021dec.onnx"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/opencv/opencv_zoo/main/models/face_detection_yunet/LICENSE" -OutFile "models\opencv_zoo\LICENSE.yunet.txt"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/opencv/opencv_zoo/main/models/face_recognition_sface/LICENSE" -OutFile "models\opencv_zoo\LICENSE.sface.txt"
```

Create `models/opencv_zoo/SOURCES.md`:

```markdown
# OpenCV Zoo Model Sources

This directory contains default models used by CCamera's `opencv_zoo` backend.

## Files

- `face_detection_yunet_2023mar.onnx`
  - Source: https://github.com/opencv/opencv_zoo/tree/main/models/face_detection_yunet
  - Direct download used by this project: https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx
  - License: MIT, copied to `LICENSE.yunet.txt`.

- `face_recognition_sface_2021dec.onnx`
  - Source: https://github.com/opencv/opencv_zoo/tree/main/models/face_recognition_sface
  - Direct download used by this project: https://github.com/opencv/opencv_zoo/raw/main/models/face_recognition_sface/face_recognition_sface_2021dec.onnx
  - License: Apache 2.0, copied to `LICENSE.sface.txt`.

These files are committed so the default Windows build can run without asking the user to provide model files.
```

- [x] **Step 4: Update default config and defaults**

Update `config/app.example.json`:

```json
{
  "camera_index": 0,
  "frame_width": 1280,
  "frame_height": 720,
  "detector_model": "models/opencv_zoo/face_detection_yunet_2023mar.onnx",
  "recognizer_model": "models/opencv_zoo/face_recognition_sface_2021dec.onnx",
  "database_path": "data/face_db.json",
  "inference_backend": "opencv_zoo",
  "detection_interval": 3,
  "detection_threshold": 0.7,
  "recognition_threshold": 0.45,
  "enrollment_consistency_threshold": 0.55,
  "enrollment_sample_count": 15,
  "enrollment_timeout_seconds": 20,
  "feature_worker_count": 2
}
```

Update the defaults in `src/core/Types.hpp`:

```cpp
    std::string detector_model = "models/opencv_zoo/face_detection_yunet_2023mar.onnx";
    std::string recognizer_model = "models/opencv_zoo/face_recognition_sface_2021dec.onnx";
    std::string database_path = "data/face_db.json";
    std::string inference_backend = "opencv_zoo";
```

Update `validate_app_config()` in `src/app/AppConfig.cpp`:

```cpp
    if (config.inference_backend != "onnxruntime_cpu" && config.inference_backend != "opencv_zoo") {
        throw std::invalid_argument("unknown inference_backend: " + config.inference_backend);
    }
```

- [x] **Step 5: Update model README**

Replace `models/README.md` with:

```markdown
# Models

The default models are committed under `models/opencv_zoo/`:

- `opencv_zoo/face_detection_yunet_2023mar.onnx`
- `opencv_zoo/face_recognition_sface_2021dec.onnx`

See `models/opencv_zoo/SOURCES.md` for original source URLs and license notes.

Custom ONNX Runtime models are still supported through `--config`, but they must match the custom `onnxruntime_cpu` contract documented in the project README.
```

- [x] **Step 6: Verify focused test passes**

Run:

```powershell
cmake --build build --config Debug --target test_app_config
ctest --test-dir build -C Debug -R test_app_config --output-on-failure
```

Expected: `test_app_config` passes.

- [x] **Step 7: Update discoveries**

Append to `docs/discoveries.md`:

```markdown
- 2026-06-08: OpenCV Zoo default model assets were added under `models/opencv_zoo`: YuNet `face_detection_yunet_2023mar.onnx` with MIT license text and SFace `face_recognition_sface_2021dec.onnx` with Apache 2.0 license text. `config/app.example.json` and `AppConfig` defaults now point at these files and use `inference_backend: "opencv_zoo"`, while `onnxruntime_cpu` remains accepted for custom configs. Focused `test_app_config` verification passed. No live camera/model manual checks were run.
```

- [x] **Step 8: Commit**

Run:

```powershell
git add config/app.example.json models src/core/Types.hpp src/app/AppConfig.cpp tests/test_app_config.cpp docs/discoveries.md
git commit -m "feat: add OpenCV Zoo default models"
```

---

### Task 2: Add Backend Interfaces And OpenCV Zoo Conversion Helpers

**Files:**
- Create: `src/face/FaceServices.hpp`
- Create: `src/face/OpenCvZooFaceDetector.hpp`
- Create: `src/face/OpenCvZooFaceDetector.cpp`
- Create: `src/face/OpenCvZooFaceFeatureExtractor.hpp`
- Create: `src/face/OpenCvZooFaceFeatureExtractor.cpp`
- Create: `tests/test_opencv_zoo_adapters.cpp`
- Modify: `src/face/FaceDetector.hpp`
- Modify: `src/face/FaceFeatureExtractor.hpp`
- Modify: `src/app/FrameRecognition.hpp`
- Modify: `src/app/FrameRecognition.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/discoveries.md`

- [ ] **Step 1: Add failing adapter tests**

Create `tests/test_opencv_zoo_adapters.cpp`:

```cpp
#include "core/Math.hpp"
#include "face/OpenCvZooFaceDetector.hpp"
#include "face/OpenCvZooFaceFeatureExtractor.hpp"

#include <cassert>
#include <opencv2/core.hpp>
#include <stdexcept>
#include <string>

namespace {

template <typename Callable>
void expect_runtime_error(Callable&& callable, const std::string& expected) {
    try {
        callable();
    } catch (const std::runtime_error& error) {
        assert(std::string(error.what()).find(expected) != std::string::npos);
        return;
    }
    assert(false);
}

bool close(float a, float b) {
    return std::abs(a - b) < 1.0e-5f;
}

}  // namespace

int main() {
    cv::Mat row(1, 15, CV_32F);
    float* values = row.ptr<float>(0);
    values[0] = 10.0f;   // x
    values[1] = 20.0f;   // y
    values[2] = 100.0f;  // width
    values[3] = 80.0f;   // height
    values[4] = 80.0f;   // right eye x
    values[5] = 45.0f;   // right eye y
    values[6] = 35.0f;   // left eye x
    values[7] = 45.0f;   // left eye y
    values[8] = 60.0f;   // nose x
    values[9] = 65.0f;   // nose y
    values[10] = 78.0f;  // right mouth x
    values[11] = 92.0f;  // right mouth y
    values[12] = 42.0f;  // left mouth x
    values[13] = 92.0f;  // left mouth y
    values[14] = 0.91f;  // score

    const auto face = ccamera::face_detection_from_yunet_row(row, 0);
    assert(close(face.box.x, 10.0f));
    assert(close(face.box.y, 20.0f));
    assert(close(face.box.width, 100.0f));
    assert(close(face.box.height, 80.0f));
    assert(close(face.score, 0.91f));
    assert(face.keypoints.left_eye == cv::Point2f(35.0f, 45.0f));
    assert(face.keypoints.right_eye == cv::Point2f(80.0f, 45.0f));
    assert(face.keypoints.nose == cv::Point2f(60.0f, 65.0f));
    assert(face.keypoints.mouth_left == cv::Point2f(42.0f, 92.0f));
    assert(face.keypoints.mouth_right == cv::Point2f(78.0f, 92.0f));

    cv::Mat bad(1, 14, CV_32F);
    expect_runtime_error(
        [&] { (void)ccamera::face_detection_from_yunet_row(bad, 0); },
        "YuNet detection output");

    cv::Mat feature = (cv::Mat_<float>(1, 4) << 3.0f, 0.0f, 4.0f, 0.0f);
    const auto embedding = ccamera::face_embedding_from_sface_feature(feature);
    assert(embedding.values.size() == 4);
    assert(close(ccamera::l2_norm(embedding.values), 1.0f));
    assert(close(embedding.values[0], 0.6f));
    assert(close(embedding.values[2], 0.8f));

    cv::Mat bad_feature(1, 0, CV_32F);
    expect_runtime_error(
        [&] { (void)ccamera::face_embedding_from_sface_feature(bad_feature); },
        "SFace feature");

    return 0;
}
```

- [ ] **Step 2: Register the failing test**

Add to `CMakeLists.txt` inside `if (CCAMERA_BUILD_TESTS)`:

```cmake
    add_executable(test_opencv_zoo_adapters tests/test_opencv_zoo_adapters.cpp)
    target_link_libraries(test_opencv_zoo_adapters PRIVATE ccamera_core)
    add_test(
        NAME test_opencv_zoo_adapters
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_opencv_zoo_adapters>
    )
```

- [ ] **Step 3: Run the test and verify it fails**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug --target test_opencv_zoo_adapters
```

Expected: build fails because `face/OpenCvZooFaceDetector.hpp` does not exist.

- [ ] **Step 4: Add backend-neutral interfaces**

Create `src/face/FaceServices.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>
#include <vector>

namespace ccamera {

class FaceDetectionService {
public:
    virtual ~FaceDetectionService() = default;
    virtual std::vector<FaceDetection> detect(const cv::Mat& frame) = 0;
};

class FaceEmbeddingService {
public:
    virtual ~FaceEmbeddingService() = default;
    virtual FaceEmbedding extract(const cv::Mat& aligned_face) = 0;
};

}  // namespace ccamera
```

Modify `src/face/FaceDetector.hpp`:

```cpp
#include "face/FaceServices.hpp"
```

and change the class declaration to:

```cpp
class FaceDetector final : public FaceDetectionService {
public:
    FaceDetector(InferenceEngine& engine, float threshold);
    std::vector<FaceDetection> detect(const cv::Mat& frame) override;
```

Modify `src/face/FaceFeatureExtractor.hpp`:

```cpp
#include "face/FaceServices.hpp"
```

and change the class declaration to:

```cpp
class FaceFeatureExtractor final : public FaceEmbeddingService {
public:
    explicit FaceFeatureExtractor(InferenceEngine& engine);
    FaceEmbedding extract(const cv::Mat& aligned_face) override;
```

Modify `src/app/FrameRecognition.hpp` so it includes `face/FaceServices.hpp` instead of `face/FaceFeatureExtractor.hpp`, and change the function signature:

```cpp
std::vector<RecognitionResult> recognize_frame_faces(
    const Frame& frame,
    const std::vector<FaceDetection>& faces,
    const FaceAligner& aligner,
    FaceEmbeddingService& extractor,
    const FaceDatabaseSnapshot& database,
    const Recognizer& recognizer);
```

Make the same signature change in `src/app/FrameRecognition.cpp`.

- [ ] **Step 5: Add OpenCV Zoo adapter headers**

Create `src/face/OpenCvZooFaceDetector.hpp`:

```cpp
#pragma once

#include "face/FaceServices.hpp"

#include <opencv2/core.hpp>
#include <opencv2/objdetect/face.hpp>

#include <string>
#include <vector>

namespace ccamera {

FaceDetection face_detection_from_yunet_row(const cv::Mat& detections, int row_index);

class OpenCvZooFaceDetector final : public FaceDetectionService {
public:
    OpenCvZooFaceDetector(const std::string& model_path, float threshold);
    std::vector<FaceDetection> detect(const cv::Mat& frame) override;

private:
    cv::Ptr<cv::FaceDetectorYN> detector_;
    cv::Size input_size_;
};

}  // namespace ccamera
```

Create `src/face/OpenCvZooFaceFeatureExtractor.hpp`:

```cpp
#pragma once

#include "face/FaceServices.hpp"

#include <opencv2/core.hpp>
#include <opencv2/objdetect/face.hpp>

#include <string>

namespace ccamera {

FaceEmbedding face_embedding_from_sface_feature(const cv::Mat& feature);

class OpenCvZooFaceFeatureExtractor final : public FaceEmbeddingService {
public:
    explicit OpenCvZooFaceFeatureExtractor(const std::string& model_path);
    FaceEmbedding extract(const cv::Mat& aligned_face) override;

private:
    cv::Ptr<cv::FaceRecognizerSF> recognizer_;
};

}  // namespace ccamera
```

- [ ] **Step 6: Add OpenCV Zoo adapter implementation**

Create `src/face/OpenCvZooFaceDetector.cpp`:

```cpp
#include "face/OpenCvZooFaceDetector.hpp"

#include <stdexcept>

namespace ccamera {

FaceDetection face_detection_from_yunet_row(const cv::Mat& detections, int row_index) {
    if (detections.empty() || detections.cols != 15 || detections.type() != CV_32F) {
        throw std::runtime_error("YuNet detection output must be a non-empty CV_32F matrix with 15 columns");
    }
    if (row_index < 0 || row_index >= detections.rows) {
        throw std::runtime_error("YuNet detection row index is out of range");
    }

    const float* row = detections.ptr<float>(row_index);
    FaceDetection face;
    face.box = cv::Rect2f(row[0], row[1], row[2], row[3]);
    face.keypoints.right_eye = cv::Point2f(row[4], row[5]);
    face.keypoints.left_eye = cv::Point2f(row[6], row[7]);
    face.keypoints.nose = cv::Point2f(row[8], row[9]);
    face.keypoints.mouth_right = cv::Point2f(row[10], row[11]);
    face.keypoints.mouth_left = cv::Point2f(row[12], row[13]);
    face.score = row[14];
    return face;
}

OpenCvZooFaceDetector::OpenCvZooFaceDetector(const std::string& model_path, float threshold)
    : detector_(cv::FaceDetectorYN::create(model_path, "", cv::Size(320, 320), threshold, 0.3f, 5000)),
      input_size_(320, 320) {
    if (detector_.empty()) {
        throw std::runtime_error("failed to create OpenCV Zoo YuNet detector from: " + model_path);
    }
}

std::vector<FaceDetection> OpenCvZooFaceDetector::detect(const cv::Mat& frame) {
    if (frame.empty()) {
        throw std::runtime_error("YuNet detector input frame is empty");
    }

    const cv::Size frame_size(frame.cols, frame.rows);
    if (frame_size != input_size_) {
        detector_->setInputSize(frame_size);
        input_size_ = frame_size;
    }

    cv::Mat detections;
    detector_->detect(frame, detections);

    std::vector<FaceDetection> faces;
    for (int row = 0; row < detections.rows; ++row) {
        faces.push_back(face_detection_from_yunet_row(detections, row));
    }
    return faces;
}

}  // namespace ccamera
```

Create `src/face/OpenCvZooFaceFeatureExtractor.cpp`:

```cpp
#include "face/OpenCvZooFaceFeatureExtractor.hpp"

#include "core/Math.hpp"

#include <stdexcept>

namespace ccamera {

FaceEmbedding face_embedding_from_sface_feature(const cv::Mat& feature) {
    if (feature.empty() || feature.type() != CV_32F) {
        throw std::runtime_error("SFace feature must be a non-empty CV_32F matrix");
    }

    std::vector<float> values;
    values.reserve(feature.total());
    if (feature.isContinuous()) {
        const float* begin = feature.ptr<float>(0);
        values.assign(begin, begin + feature.total());
    } else {
        for (int row = 0; row < feature.rows; ++row) {
            const float* ptr = feature.ptr<float>(row);
            values.insert(values.end(), ptr, ptr + feature.cols);
        }
    }

    if (values.empty()) {
        throw std::runtime_error("SFace feature values are empty");
    }
    return {l2_normalized(values)};
}

OpenCvZooFaceFeatureExtractor::OpenCvZooFaceFeatureExtractor(const std::string& model_path)
    : recognizer_(cv::FaceRecognizerSF::create(model_path, "")) {
    if (recognizer_.empty()) {
        throw std::runtime_error("failed to create OpenCV Zoo SFace recognizer from: " + model_path);
    }
}

FaceEmbedding OpenCvZooFaceFeatureExtractor::extract(const cv::Mat& aligned_face) {
    if (aligned_face.empty()) {
        throw std::runtime_error("SFace recognizer input face is empty");
    }
    cv::Mat feature;
    recognizer_->feature(aligned_face, feature);
    return face_embedding_from_sface_feature(feature);
}

}  // namespace ccamera
```

- [ ] **Step 7: Link OpenCV objdetect and register sources**

Update `find_package` in `CMakeLists.txt`:

```cmake
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio objdetect)
```

Add `opencv_objdetect` to the OpenCV runtime target list in both existing `foreach (_opencv_runtime_target IN ITEMS ...)` blocks:

```cmake
            opencv_objdetect
```

Add new source files to `ccamera_core`:

```cmake
    src/face/OpenCvZooFaceDetector.cpp
    src/face/OpenCvZooFaceFeatureExtractor.cpp
```

Add `opencv_objdetect` to `target_link_libraries(ccamera_core PUBLIC ...)`:

```cmake
target_link_libraries(ccamera_core PUBLIC
    opencv_objdetect
)
```

- [x] **Step 8: Verify adapter test passes**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug --target test_opencv_zoo_adapters
ctest --test-dir build -C Debug -R test_opencv_zoo_adapters --output-on-failure
```

Expected: `test_opencv_zoo_adapters` passes.

- [ ] **Step 9: Run existing focused tests affected by interface change**

Run:

```powershell
cmake --build build --config Debug --target test_offline_virtual_dataset test_face_feature_extraction test_visualization_contract
ctest --test-dir build -C Debug -R "test_offline_virtual_dataset|test_face_feature_extraction|test_visualization_contract" --output-on-failure
```

Expected: all focused tests pass.

- [ ] **Step 10: Update discoveries**

Append to `docs/discoveries.md`:

```markdown
- 2026-06-08: Added backend-neutral face detection/embedding service interfaces and OpenCV Zoo adapter helpers. YuNet output rows are converted from OpenCV `FaceDetectorYN` order `[x, y, w, h, right_eye, left_eye, nose, right_mouth, left_mouth, score]` into project `FaceDetection` fields, and SFace features are converted into L2-normalized `FaceEmbedding` values. Focused adapter and affected recognition/visualization tests passed. No live camera/model manual checks were run.
```

- [ ] **Step 11: Commit**

Run:

```powershell
git add CMakeLists.txt src/app/FrameRecognition.hpp src/app/FrameRecognition.cpp src/face tests/test_opencv_zoo_adapters.cpp docs/discoveries.md
git commit -m "feat: add OpenCV Zoo face adapters"
```

---

### Task 3: Wire Backend Selection Into AppController

**Files:**
- Modify: `src/app/AppController.hpp`
- Modify: `src/app/AppController.cpp`
- Modify: `tests/test_app_run_cleanup.cpp`
- Modify: `docs/discoveries.md`

- [ ] **Step 1: Add failing backend construction coverage**

Open `tests/test_app_run_cleanup.cpp` and add these includes:

```cpp
#include "app/AppController.hpp"

#include <string>
```

Add this block before the final `return 0;`:

```cpp
    bool saw_missing_model_error = false;
    {
        ccamera::AppConfig config;
        config.inference_backend = "opencv_zoo";
        config.detector_model = "models/missing-yunet.onnx";
        config.recognizer_model = "models/missing-sface.onnx";
        try {
            ccamera::AppController app(config);
            (void)app;
        } catch (const std::exception& error) {
            const std::string message = error.what();
            saw_missing_model_error =
                message.find("missing-yunet.onnx") != std::string::npos ||
                message.find("missing-sface.onnx") != std::string::npos;
        }
    }
    assert(saw_missing_model_error);
```

- [ ] **Step 2: Run focused test and verify it fails or exposes current hard-coded backend**

Run:

```powershell
cmake --build build --config Debug --target test_app_run_cleanup
ctest --test-dir build -C Debug -R test_app_run_cleanup --output-on-failure
```

Expected: failure before implementation because `AppController` still constructs `OnnxRuntimeCpuEngine` unconditionally.

- [ ] **Step 3: Change AppController member ownership**

Update includes in `src/app/AppController.hpp`:

```cpp
#include "face/FaceServices.hpp"
#include "inference/OnnxRuntimeCpuEngine.hpp"

#include <memory>
```

Replace these members:

```cpp
    OnnxRuntimeCpuEngine detector_engine_;
    OnnxRuntimeCpuEngine recognizer_engine_;
    FaceDetector detector_;
    FaceAligner aligner_;
    FaceFeatureExtractor extractor_;
```

with:

```cpp
    std::unique_ptr<OnnxRuntimeCpuEngine> detector_engine_;
    std::unique_ptr<OnnxRuntimeCpuEngine> recognizer_engine_;
    std::unique_ptr<FaceDetectionService> detector_;
    FaceAligner aligner_;
    std::unique_ptr<FaceEmbeddingService> extractor_;
```

- [ ] **Step 4: Add backend factory logic**

Update includes in `src/app/AppController.cpp`:

```cpp
#include "face/OpenCvZooFaceDetector.hpp"
#include "face/OpenCvZooFaceFeatureExtractor.hpp"
```

Add helper functions inside `namespace ccamera { namespace { ... } }` before the constructor:

```cpp
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
```

Then replace the constructor initializer and body with:

```cpp
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
```

- [ ] **Step 5: Update call sites**

In `handle_enrollment()`, replace:

```cpp
        const FaceEmbedding sample = extractor_.extract(aligned);
```

with:

```cpp
        const FaceEmbedding sample = extractor_->extract(aligned);
```

In `process_frame()`, replace:

```cpp
    const auto faces = detector_.detect(frame.image);
```

with:

```cpp
    const auto faces = detector_->detect(frame.image);
```

Replace the `recognize_frame_faces()` call:

```cpp
    return recognize_frame_faces(frame, faces, aligner_, extractor_, db_snapshot, recognizer_);
```

with:

```cpp
    return recognize_frame_faces(frame, faces, aligner_, *extractor_, db_snapshot, recognizer_);
```

- [ ] **Step 6: Verify backend construction coverage passes**

Run:

```powershell
cmake --build build --config Debug --target test_app_run_cleanup
ctest --test-dir build -C Debug -R test_app_run_cleanup --output-on-failure
```

Expected: `test_app_run_cleanup` passes.

- [ ] **Step 7: Run app-level focused tests**

Run:

```powershell
cmake --build build --config Debug --target test_offline_virtual_dataset test_enrollment_session test_recognition_results
ctest --test-dir build -C Debug -R "test_offline_virtual_dataset|test_enrollment_session|test_recognition_results" --output-on-failure
```

Expected: all focused tests pass.

- [ ] **Step 8: Update discoveries**

Append to `docs/discoveries.md`:

```markdown
- 2026-06-08: `AppController` now selects face backend by `AppConfig::inference_backend`. The default `opencv_zoo` backend constructs OpenCV Zoo YuNet/SFace adapters, while `onnxruntime_cpu` keeps the existing `OnnxRuntimeCpuEngine` path for custom configs. Focused backend construction and app recognition tests passed. No live camera/model manual checks were run.
```

- [ ] **Step 9: Commit**

Run:

```powershell
git add src/app/AppController.hpp src/app/AppController.cpp tests/test_app_run_cleanup.cpp docs/discoveries.md
git commit -m "feat: wire OpenCV Zoo backend"
```

---

### Task 4: Make No-Argument Startup Enter Runtime Directly

**Files:**
- Create: `src/app/StartupMode.hpp`
- Create: `src/app/StartupMode.cpp`
- Create: `tests/test_startup_mode.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/discoveries.md`

- [ ] **Step 1: Add failing startup-mode test**

Create `tests/test_startup_mode.cpp`:

```cpp
#include "app/StartupMode.hpp"

#include <cassert>

int main() {
    const char* no_args[] = {"ccamera.exe"};
    const auto default_start = ccamera::classify_startup_mode(1, const_cast<char**>(no_args));
    assert(default_start == ccamera::StartupMode::DefaultRuntime);

    const char* config_args[] = {"ccamera.exe", "--config", "config/app.example.json"};
    const auto config_start = ccamera::classify_startup_mode(3, const_cast<char**>(config_args));
    assert(config_start == ccamera::StartupMode::ConfigRuntime);

    const char* positional_args[] = {"ccamera.exe", "config/app.example.json"};
    const auto positional_start = ccamera::classify_startup_mode(2, const_cast<char**>(positional_args));
    assert(positional_start == ccamera::StartupMode::ConfigRuntime);

    return 0;
}
```

- [ ] **Step 2: Register failing startup-mode test**

Add `src/app/StartupMode.cpp` to `ccamera_config`:

```cmake
add_library(ccamera_config
    src/app/AppConfig.cpp
    src/app/StartupMode.cpp
)
```

Add the test inside `if (CCAMERA_BUILD_TESTS)`:

```cmake
    add_executable(test_startup_mode tests/test_startup_mode.cpp)
    target_link_libraries(test_startup_mode PRIVATE ccamera_config)
    add_test(NAME test_startup_mode COMMAND test_startup_mode)
```

- [ ] **Step 3: Run startup-mode test and verify it fails**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug --target test_startup_mode
```

Expected: build fails because `app/StartupMode.hpp` does not exist.

- [ ] **Step 4: Add startup-mode helper**

Create `src/app/StartupMode.hpp`:

```cpp
#pragma once

namespace ccamera {

enum class StartupMode {
    DefaultRuntime,
    ConfigRuntime,
};

StartupMode classify_startup_mode(int argc, char** argv);

}  // namespace ccamera
```

Create `src/app/StartupMode.cpp`:

```cpp
#include "app/StartupMode.hpp"

namespace ccamera {

StartupMode classify_startup_mode(int argc, char**) {
    if (argc <= 1) {
        return StartupMode::DefaultRuntime;
    }
    return StartupMode::ConfigRuntime;
}

}  // namespace ccamera
```

- [ ] **Step 5: Change launcher no-argument flow**

In `src/main.cpp`, add:

```cpp
#include "app/StartupMode.hpp"
```

In `main()`, replace the current `if (argc <= 1) { ... show_startup_window ... } else { ... }` block with:

```cpp
        const ccamera::StartupMode startup_mode = ccamera::classify_startup_mode(argc, argv);
        if (startup_mode == ccamera::StartupMode::DefaultRuntime) {
            config = default_startup_config();
            generated_args = {
                argv != nullptr && argv[0] != nullptr ? argv[0] : "ccamera.exe",
            };
            generated_argv = mutable_argv(generated_args);
            argc = static_cast<int>(generated_argv.size());
            argv = generated_argv.data();
        } else {
            config = ccamera::load_app_config_from_args(argc, argv);
        }
```

Remove unused startup-window-only code from `src/main.cpp`:

```cpp
#include "ui/StartupWindow.hpp"
```

Remove the `temporary_startup_config_path()` function and the `TemporaryConfigCleanup` class from `src/main.cpp`. Remove these local variables from `main()`:

```cpp
        TemporaryConfigCleanup temporary_config;
```

- [ ] **Step 6: Verify startup-mode test passes**

Run:

```powershell
cmake --build build --config Debug --target test_startup_mode
ctest --test-dir build -C Debug -R test_startup_mode --output-on-failure
```

Expected: `test_startup_mode` passes.

- [ ] **Step 7: Verify launcher still handles config arguments**

Run:

```powershell
cmake --build build --config Debug --target ccamera
.\build\ccamera.exe --config config\app.example.json
```

Expected: The process no longer fails with a missing default model error. If it reaches camera startup and cannot open the camera, the error is a controlled `ccamera:` application error. Do not mark camera preview as manually passed.

- [ ] **Step 8: Update discoveries**

Append to `docs/discoveries.md`:

```markdown
- 2026-06-08: No-argument startup now classifies as default runtime startup instead of startup-window configuration. `--config` and positional config startup remain runtime paths. Automated `test_startup_mode` passed, and launcher startup reached controlled application/runtime handling with committed default models. No live camera/model manual checks were run.
```

- [ ] **Step 9: Commit**

Run:

```powershell
git add CMakeLists.txt src/app/StartupMode.hpp src/app/StartupMode.cpp src/main.cpp tests/test_startup_mode.cpp docs/discoveries.md
git commit -m "feat: start runtime by default"
```

---

### Task 5: Update Startup CTest Probes For Committed Models

**Files:**
- Modify: `cmake/CheckExecutableStartup.cmake`
- Modify: `CMakeLists.txt`
- Modify: `docs/discoveries.md`

- [ ] **Step 1: Add missing-model startup probe mode**

Modify `cmake/CheckExecutableStartup.cmake` so it supports `CCAMERA_STARTUP_MODE=missing_model`. Add this block after `set(_config_file "${CCAMERA_CONFIG_FILE}")`:

```cmake
if (CCAMERA_STARTUP_MODE STREQUAL "missing_model")
    if (NOT DEFINED CCAMERA_STARTUP_PROBE_DIR)
        message(FATAL_ERROR "CCAMERA_STARTUP_PROBE_DIR is required for missing_model startup mode")
    endif()

    file(REMOVE_RECURSE "${CCAMERA_STARTUP_PROBE_DIR}")
    file(MAKE_DIRECTORY "${CCAMERA_STARTUP_PROBE_DIR}")
    set(_database_path "${CCAMERA_STARTUP_PROBE_DIR}/face_db.json")
    set(_config_file "${CCAMERA_STARTUP_PROBE_DIR}/app-missing-model.json")
    file(TO_CMAKE_PATH "${_database_path}" _database_path_json)
    file(WRITE "${_config_file}" [=[
{
  "camera_index": 0,
  "frame_width": 1280,
  "frame_height": 720,
  "detector_model": "models/missing-default-detector.onnx",
  "recognizer_model": "models/missing-default-recognizer.onnx",
  "database_path": "@DATABASE_PATH@",
  "inference_backend": "opencv_zoo",
  "detection_interval": 3,
  "detection_threshold": 0.7,
  "recognition_threshold": 0.45,
  "enrollment_consistency_threshold": 0.55,
  "enrollment_sample_count": 15,
  "enrollment_timeout_seconds": 20,
  "feature_worker_count": 2
}
]=])
    file(READ "${_config_file}" _config_json)
    string(REPLACE "@DATABASE_PATH@" "${_database_path_json}" _config_json "${_config_json}")
    file(WRITE "${_config_file}" "${_config_json}")
endif()
```

After the generic `_error MATCHES "ccamera:"` handling, add a specific missing-model check:

```cmake
if (CCAMERA_STARTUP_MODE STREQUAL "missing_model")
    if (_error MATCHES "detector model does not exist: models/missing-default-detector.onnx")
        message(STATUS "Executable reported expected missing default model error: ${_error}")
        return()
    endif()
    message(FATAL_ERROR
        "Missing-model startup probe did not report the expected missing detector model. "
        "result=${_result}; stdout='${_output}'; stderr='${_error}'"
    )
endif()
```

Place the missing-model check before the broad `_error MATCHES "ccamera:"` success branch so a generic `ccamera:` error cannot mask the wrong failure.

- [ ] **Step 2: Keep runtime dummy mode on custom ONNX backend**

In the existing `runtime` mode JSON block, keep:

```json
  "inference_backend": "onnxruntime_cpu",
```

This preserves the dummy invalid ONNX model probe for runtime-module load behavior without exercising live camera.

- [ ] **Step 3: Update CTest registration**

In `CMakeLists.txt`, update `test_executable_startup` to pass the new missing-model mode:

```cmake
        add_test(
            NAME test_executable_startup
            COMMAND ${CMAKE_COMMAND}
                -DCCAMERA_TARGET_FILE=$<TARGET_FILE:ccamera>
                -DCCAMERA_CONFIG_FILE=${CMAKE_CURRENT_SOURCE_DIR}/config/app.example.json
                -DCCAMERA_WORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}
                -DCCAMERA_STARTUP_MODE=missing_model
                -DCCAMERA_STARTUP_PROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}/manual-startup-probe
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckExecutableStartup.cmake"
        )
```

- [ ] **Step 4: Run focused startup tests**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug --target ccamera
ctest --test-dir build -C Debug -R "test_executable_startup|test_executable_startup_runtime|test_runtime_deployment" --output-on-failure
```

Expected: all three tests pass. `test_executable_startup` should validate missing default model error using a generated config, while `test_executable_startup_runtime` should still reject runtime module load/export failures.

- [ ] **Step 5: Update discoveries**

Append to `docs/discoveries.md`:

```markdown
- 2026-06-08: Windows startup CTest probes were updated for committed default models. `test_executable_startup` now uses a generated missing-model config so it still validates clear missing model errors, and `test_executable_startup_runtime` keeps the dummy invalid ONNX Runtime path to verify runtime-module loading. Focused startup/runtime deployment tests passed. No live camera/model manual checks were run.
```

- [ ] **Step 6: Commit**

Run:

```powershell
git add CMakeLists.txt cmake/CheckExecutableStartup.cmake docs/discoveries.md
git commit -m "test: update startup probes for default models"
```

---

### Task 6: Documentation, Final Verification, And Manual Checklist Preservation

**Files:**
- Modify: `README.md`
- Modify: `docs/superpowers/plans/2026-06-04-cpp-camera-face-recognition.md`
- Modify: `docs/discoveries.md`

- [ ] **Step 1: Update README running instructions**

In `README.md`, update the `Running` section to say:

```markdown
## Running

1. Build the executable.
2. Double-click `build/ccamera.exe`, or run it from PowerShell:

   ```powershell
   .\build\ccamera.exe
   ```

The default configuration uses the committed OpenCV Zoo YuNet and SFace models under `models/opencv_zoo/`. The app opens the live preview directly when launched without arguments.

Use `ccamera.exe --config config/app.example.json` or another JSON file when you need to customize model paths, camera index, thresholds, or database path.
```

Keep the manual operation text for Enroll, Delete, List, Quit, `Q`, and `Esc`. Keep the custom ONNX Runtime model contract section, but label it as applying to `inference_backend: "onnxruntime_cpu"` custom configs.

- [ ] **Step 2: Update main implementation plan acceptance notes**

In `docs/superpowers/plans/2026-06-04-cpp-camera-face-recognition.md`, add a new section near the existing Simple Interactive GUI Support section:

```markdown
### Direct Usable OpenCV Zoo Default

- [x] **Step 1: Add committed OpenCV Zoo default models**
- [x] **Step 2: Add OpenCV Zoo adapter layer**
- [x] **Step 3: Wire `opencv_zoo` backend into runtime**
- [x] **Step 4: Start runtime directly on no-argument launch**
- [x] **Step 5: Update startup/runtime probes**
- [x] **Step 6: Run automated verification**

Progress note 2026-06-08: The default build now uses committed OpenCV Zoo YuNet/SFace models under `models/opencv_zoo`, starts the runtime path directly when `ccamera.exe` is launched with no arguments, and preserves `--config` for custom configs. Automated verification passed with configure, build, focused startup/runtime tests, and full CTest. Manual camera preview, real face detection, live enrollment, and real recognition checks were not run and remain unchecked.
```

Do not check any manual acceptance checklist item unless a manual test was actually run.

- [ ] **Step 3: Run full verification**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected:

```text
100% tests passed
```

- [ ] **Step 4: Probe default executable startup without claiming manual pass**

Run:

```powershell
.\build\ccamera.exe
```

Expected: The executable should no longer fail with missing model errors. If it opens a live preview, record that the process reached preview manually only if a human actually observes it. If the environment lacks a usable camera or the app is stopped before visual confirmation, record the exact controlled error and keep manual checklist items unchecked.

- [ ] **Step 5: Update final discoveries**

Append to `docs/discoveries.md`:

```markdown
- 2026-06-08: Direct usable OpenCV Zoo default work completed automated verification with `cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON`, `cmake --build build --config Debug`, and `ctest --test-dir build -C Debug --output-on-failure`. The default no-argument launcher path uses committed OpenCV Zoo model files and no longer fails at missing model validation. Manual live camera preview, live enrollment, and real recognition quality checks were not run and remain unchecked.
```

If the default executable probe produced a controlled camera/model/runtime error, include the exact error in the discovery note instead of saying preview passed.

- [ ] **Step 6: Commit**

Run:

```powershell
git add README.md docs/superpowers/plans/2026-06-04-cpp-camera-face-recognition.md docs/discoveries.md
git commit -m "docs: update direct usable run instructions"
```

---

## Final Acceptance

Automated acceptance requires:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Manual acceptance remains deferred unless actually performed:

- [ ] Camera preview opens.
- [ ] Face rectangles are drawn around detected faces.
- [ ] Red keypoint markers are drawn on detected face landmarks.
- [ ] `enroll <name>` stores a live camera enrollment.
- [ ] `list` shows enrolled identities.
- [ ] Recognition displays the enrolled name when similarity passes the configured threshold.
- [ ] Unknown faces are labeled `Unknown`.
- [ ] `delete <name>` removes the identity from the local database.
- [ ] `quit`, `Q`, and `Esc` stop the application.
