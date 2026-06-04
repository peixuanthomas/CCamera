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
