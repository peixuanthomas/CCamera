# Direct Usable OpenCV Zoo Default Design

## Goal

Make the current Windows project build into a directly usable face-recognition executable on this workstation. After the user manually builds the project, double-clicking `ccamera.exe` should enter the camera preview and runtime controls without requiring the user to choose model files first.

The default model set is OpenCV Zoo YuNet for face detection and OpenCV Zoo SFace for face recognition. The model files are committed under `models/` with license and source documentation so the default configuration works from the repository.

## Scope

In scope:

- Add OpenCV Zoo YuNet and SFace model files to the repository.
- Add model source and license documentation beside the models.
- Update default configuration so it points at the committed OpenCV Zoo model paths.
- Add an OpenCV Zoo model adapter layer that converts YuNet/SFace behavior into the existing `FaceDetection` and `FaceEmbedding` domain types.
- Preserve the existing app structure, local database, enrollment flow, runtime controls, and command-line config support.
- Change no-argument double-click startup to use the default config and enter preview directly.
- Keep `--config` startup for custom paths and diagnostics.
- Add automated tests for default config, startup routing, model presence, adapter conversion, runtime deployment, and clear errors.
- Update README, implementation plan checkboxes, and discovery notes.
- Commit each implementation task after its verification passes.

Out of scope:

- Clean-machine dependency bootstrapping.
- Installer creation or release packaging.
- GPU inference.
- Commercial licensing review.
- Cross-platform packaging.
- UI redesign beyond the default startup behavior.
- Marking live camera, real face recognition, enrollment, or GUI interaction as manually passed.

## Constraints

This work targets the current Windows development workstation and its existing dependency layout. Project-local OpenCV, nlohmann/json, ONNX Runtime, and runtime DLL deployment behavior should continue to work as currently configured. Explicit user-supplied dependency paths should still take precedence over local fallbacks.

Manual camera and real model behavior checks are intentionally deferred. The implementation must not fabricate camera, model, or recognition results, and must leave manual acceptance checklist items unchecked until they are actually run.

## Model Assets

The repository will contain:

- `models/opencv_zoo/face_detection_yunet_2023mar.onnx`
- `models/opencv_zoo/face_recognition_sface_2021dec.onnx`
- License files for the model directories.
- A source note documenting original OpenCV Zoo URLs, model names, and expected use.

The committed model filenames become the default paths in `config/app.example.json`.

## Architecture

The existing high-level structure remains:

- `ccamera.exe` is a lightweight launcher.
- `ccamera_runtime.dll` owns camera, preview-window, and runtime dependencies.
- `AppController` coordinates camera frames, detection, recognition, database matching, enrollment, and preview results.
- `FaceDatabase`, `Recognizer`, `EnrollmentSession`, `RecognitionResultsStore`, and `RuntimeControls` remain reusable.

The implementation adds a focused OpenCV Zoo adapter layer. The adapter layer translates YuNet/SFace model-specific inputs and outputs into current application types. The rest of the app should not depend on raw OpenCV Zoo tensor details.

Possible file names:

- `src/face/OpenCvZooFaceDetector.hpp`
- `src/face/OpenCvZooFaceDetector.cpp`
- `src/face/OpenCvZooFaceFeatureExtractor.hpp`
- `src/face/OpenCvZooFaceFeatureExtractor.cpp`
- Supporting model contract helpers if needed.

The adapter layer should be small and explicit:

- YuNet rows are converted into `FaceDetection` boxes and five keypoints in the order expected by preview and alignment code.
- SFace output is converted into `FaceEmbedding`, then existing normalization and matching behavior is reused.
- Errors include enough context to identify the model, output name, and observed shape.

The existing custom ONNX Runtime pipeline may remain available for `--config` and future extension, but the default path should use the OpenCV Zoo adapter.

## Startup Behavior

When launched with no arguments, `ccamera.exe` should:

1. Load the default startup config from `config/app.example.json` or equivalent built-in defaults.
2. Validate that the default OpenCV Zoo model files exist.
3. Load `ccamera_runtime.dll`.
4. Enter the live preview path directly.

The startup configuration window remains available in the codebase but is not the default double-click path. Command-line startup with `--config <path>` and positional config startup continue to work for custom model paths and diagnostics.

If startup fails before preview, the error message should be concise and actionable:

- Missing model: name the missing file path.
- Runtime module load failure: retain the current DLL guidance.
- Camera open failure: report the current camera status error.
- Model contract failure: name the model stage and observed output issue.

## Runtime User Flow

Default runtime flow:

1. User double-clicks `ccamera.exe`.
2. Preview opens using camera index `0`.
3. Faces are detected with YuNet.
4. Rectangles and red keypoints are drawn using existing preview overlay behavior.
5. SFace embeddings are matched against the local face database.
6. Unknown faces are labeled `Unknown`.
7. The right-side controls support enroll, delete, list, and quit.

Enrollment remains live-camera based. The database path remains local and should be created on demand by existing database save behavior.

## Automated Testing

Automated tests should cover:

- Default config points to existing committed model files.
- No-argument startup routes to runtime instead of the startup configuration window.
- Missing default model errors include the missing path.
- YuNet row conversion produces the expected `FaceDetection` box, score, and five keypoints.
- SFace embedding conversion produces a valid `FaceEmbedding` compatible with existing recognition.
- Existing offline virtual dataset and runtime controls tests still pass.
- Windows runtime deployment tests still pass.
- Dummy-model startup tests are updated so they continue to distinguish runtime-module failures from downstream model/config/camera errors.

Automated tests must not claim live camera or real ONNX recognition quality has passed. They may verify startup routing and model file presence.

## Manual Testing Deferred

The following remain manual and unchecked until later:

- Camera preview opens on real hardware.
- Face rectangles are drawn around real detected faces.
- Red keypoint markers appear on real landmarks.
- Live enrollment stores an identity.
- Recognition displays an enrolled name above threshold.
- Unknown faces are labeled `Unknown`.
- Delete removes an enrolled identity.
- Quit, `Q`, and `Esc` stop the application.

## Verification

Before implementation work is considered complete, run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The final result should be a build output where `build/ccamera.exe` can be double-clicked to enter the runtime path using the committed default models, subject to later manual camera/model validation.
