# CCamera Face Recognition

C++ camera face recognition prototype using OpenCV for camera/display and OpenCV Zoo YuNet/SFace by default, with custom ONNX Runtime CPU configs retained.

First-version features:

- Live camera capture.
- Face rectangles and red face keypoint markers in the preview window.
- Embedding-based local identity recognition.
- Live camera enrollment.
- Local embedding database.

Build dependencies:

- CMake 3.20 or newer.
- OpenCV with `core`, `imgproc`, `highgui`, `videoio`, and `objdetect`.
- ONNX Runtime CPU.
- nlohmann/json.

Configure with `-DONNXRUNTIME_ROOT=<path-to-onnxruntime>`.

Example:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON -DONNXRUNTIME_ROOT=<path-to-onnxruntime> -DCMAKE_PREFIX_PATH=<opencv-and-json-package-prefix>
```

On Windows, use semicolon-separated entries in `CMAKE_PREFIX_PATH` when OpenCV and nlohmann/json are installed under different prefixes.

When project-local dependencies exist under `third_party`, CMake uses them as fallback hints. The bundled MSYS2 OpenCV and nlohmann/json prefix is only used for MinGW builds, and explicit `OpenCV_DIR`, `ONNXRUNTIME_ROOT`, or `CMAKE_PREFIX_PATH` values take precedence.

On Windows, `ccamera.exe` is a lightweight launcher. It validates the config and model paths first, then loads `ccamera_runtime.dll`, which owns the camera and preview-window path. The build deploys the configured OpenCV runtime DLLs, the configured ONNX Runtime DLL, the MinGW runtime DLLs, and transitive DLL imports resolved from the configured prefixes beside the executable. The configured prefixes must contain the full OpenCV GUI/video backend runtime set, including any Qt, FFmpeg, GStreamer, image-codec, and secondary codec DLLs imported by that OpenCV build. CTest reports unresolved transitive DLL names when they are absent from the configured prefixes.

## Running

1. Build the executable first.
2. Double-click `build/ccamera.exe`, or run it from PowerShell:

   ```powershell
   .\build\ccamera.exe
   ```

The default configuration uses the committed OpenCV Zoo YuNet and SFace models under `models/opencv_zoo/`. The app opens the live preview directly when launched without arguments.

Use `ccamera.exe --config config/app.example.json` or another JSON file when you need to customize model paths, camera index, thresholds, or database path.

If model files are missing, the launcher reports the missing model path before loading camera or preview dependencies.

The preview window draws rectangles around detected faces and red points on detected face keypoints. The command line accepts `enroll <name>`, `delete <name>`, `list`, and `quit`.

The preview window also includes a right-side control panel:

- `Enroll`: type a name in the preview window and press Enter to start live enrollment.
- `Delete`: type a name in the preview window and press Enter to remove that identity.
- `List`: prints enrolled identities to the console; the panel also shows enrolled names and template counts.
- `Quit`: stops the app.

Press `Q` or `Esc` in the preview window to close the app when the panel is not asking for a name. Press `Esc` while entering a name to cancel that input.

## Runtime Browser UI

When the app starts, it also starts a local browser UI bound to `127.0.0.1` on an automatically selected port. The console prints the URL:

```text
Web UI: http://127.0.0.1:<port>
```

Open that URL in a local browser to view the live MJPEG preview, current recognition status, enrolled people, and runtime controls. The OpenCV preview window remains open and fully usable.

The browser UI can enqueue `Enroll`, `Delete`, `List`, and `Quit` commands. Enrollment and deletion still use the existing local database flow and persist to the configured `database_path`, normally `data/face_db.json`. The UI is localhost-only; it does not provide LAN or remote access.

## Offline Virtual Dataset Test

The automated test suite includes a no-camera virtual dataset path:

```powershell
ctest --test-dir build -C Debug -R test_offline_virtual_dataset --output-on-failure
```

This test uses a synthetic OpenCV frame and test-only fake detector/recognizer engines. The fake recognizer derives its embedding from the aligned synthetic crop pixels, so the test checks detector output decoding, keypoint decoding, face alignment, feature extraction normalization, database snapshot matching, and recognition result assembly without opening camera hardware or loading ONNX model files.

## Custom ONNX Runtime Model Contract

This contract applies to custom configs that set `inference_backend: "onnxruntime_cpu"`.

The detector input is `image`: a batch-1 NCHW float32 tensor using OpenCV BGR channel order. The detector output is `detections` with shape `[N, 15]`; each row is `[x1, y1, x2, y2, score, left_eye_x, left_eye_y, right_eye_x, right_eye_y, nose_x, nose_y, mouth_left_x, mouth_left_y, mouth_right_x, mouth_right_y]` in frame pixel coordinates.

The recognizer input is `face`: a batch-1 NCHW float32 aligned face tensor using OpenCV BGR channel order. The recognizer output is `embedding` with shape `[N]` or `[1, N]`. Embeddings are L2-normalized before matching or storage.
