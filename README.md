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

Example:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON -DONNXRUNTIME_ROOT=<path-to-onnxruntime> -DCMAKE_PREFIX_PATH=<opencv-and-json-package-prefix>
```

On Windows, use semicolon-separated entries in `CMAKE_PREFIX_PATH` when OpenCV and nlohmann/json are installed under different prefixes.

When project-local dependencies exist under `third_party`, CMake uses them as fallback hints. The bundled MSYS2 OpenCV and nlohmann/json prefix is only used for MinGW builds, and explicit `OpenCV_DIR`, `ONNXRUNTIME_ROOT`, or `CMAKE_PREFIX_PATH` values take precedence.

## Running

1. Place compatible detector and recognizer ONNX files in `models/`.
2. Configure dependency paths with CMake.
3. Build the executable.
4. Run `ccamera.exe --config config/app.example.json`, or omit `--config` to use built-in defaults.

The preview window draws rectangles around detected faces and red points on detected face keypoints. The command line accepts `enroll <name>`, `delete <name>`, `list`, and `quit`.

Press `Q` or `Esc` in the preview window to close the app.

## Offline Virtual Dataset Test

The automated test suite includes a no-camera virtual dataset path:

```powershell
ctest --test-dir build -C Debug -R test_offline_virtual_dataset --output-on-failure
```

This test uses a synthetic OpenCV frame and test-only fake detector/recognizer engines. The fake recognizer derives its embedding from the aligned synthetic crop pixels, so the test checks detector output decoding, keypoint decoding, face alignment, feature extraction normalization, database snapshot matching, and recognition result assembly without opening camera hardware or loading ONNX model files.

## Model Contract

The detector input is `image`: a batch-1 NCHW float32 tensor using OpenCV BGR channel order. The detector output is `detections` with shape `[N, 15]`; each row is `[x1, y1, x2, y2, score, left_eye_x, left_eye_y, right_eye_x, right_eye_y, nose_x, nose_y, mouth_left_x, mouth_left_y, mouth_right_x, mouth_right_y]` in frame pixel coordinates.

The recognizer input is `face`: a batch-1 NCHW float32 aligned face tensor using OpenCV BGR channel order. The recognizer output is `embedding` with shape `[N]` or `[1, N]`. Embeddings are L2-normalized before matching or storage.
