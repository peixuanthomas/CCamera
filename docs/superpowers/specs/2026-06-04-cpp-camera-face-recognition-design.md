# C++ Camera Face Recognition Design

## Goal

Build a C++ desktop program that uses a computer camera to perform real-time face detection and identity recognition. The first implementation targets CPU inference for simpler deployment, while keeping the inference layer replaceable so GPU backends can be added later.

The system must support the full recognition flow:

1. Capture frames from the local camera.
2. Detect faces in the frame.
3. Extract face embedding vectors.
4. Compare embeddings against a local enrolled face database.
5. Display the recognized identity, confidence, FPS, enrollment status, face boxes, and visible face keypoints.
6. Enroll new identities from the live camera feed.

## Chosen Approach

Use OpenCV for camera access, frame processing, and preview display. Use ONNX Runtime for model inference. This keeps camera and image handling simple while giving the inference layer a clean path from CPU execution to CUDA, DirectML, TensorRT, or other ONNX Runtime execution providers later.

Alternative approaches were considered:

- OpenCV DNN only: simpler dependency surface, but weaker inference backend flexibility.
- Traditional OpenCV/dlib recognition: easier for demos, but weaker accuracy and robustness.

The selected OpenCV + ONNX Runtime approach best matches the requirements for real-time performance, modern face recognition models, C++ support, and future acceleration.

## Application Shape

The first version is a command-line service with a camera preview window.

The command line controls service actions:

- `start`: start camera capture and recognition.
- `enroll <name>`: enter live enrollment mode for a person.
- `delete <name>`: remove a person from the local face database.
- `list`: list enrolled identities.
- `quit`: stop the program.

The preview window displays the live camera feed, face boxes, face keypoints, identity labels, confidence scores, FPS, and enrollment progress. The preview window is not the primary control surface; it only provides visual feedback.

Face visualization requirements:

- Draw one rectangle around each detected face.
- Draw red points on detected face keypoint positions, such as eyes, nose, and mouth corners when the detector or landmark model provides them.
- Keep embedding vectors internal to recognition. Numeric embedding values are not drawn directly because they are high-dimensional recognition features, not image positions.
- If the selected detector does not return landmarks, the first version should either omit red points or use a lightweight landmark model behind the same component boundary.

## Core Components

### CameraCapture

Owns `cv::VideoCapture` and continuously reads frames from the configured camera index. It publishes the latest frame into a small bounded buffer. The capture thread must not block on detection or recognition work.

Responsibilities:

- Open and validate the camera device.
- Read frames at the camera's available rate.
- Timestamp frames.
- Drop old frames when workers fall behind.
- Report camera failures to the application controller.

### FrameBuffer

Stores the latest camera frame and exposes thread-safe read/write access. The first version should use a "latest frame wins" strategy instead of an unbounded queue, because real-time recognition should prefer fresh frames over processing stale frames.

Responsibilities:

- Store the most recent frame.
- Provide synchronization around frame access.
- Track frame sequence numbers for worker coordination.

### FaceDetector

Runs a face detection ONNX model through the inference abstraction. It returns bounding boxes, detection scores, and optional facial landmarks if the selected model provides them.

Responsibilities:

- Preprocess frames for the detector model.
- Run detector inference.
- Decode boxes and confidence scores.
- Decode landmark or keypoint coordinates when the model provides them.
- Apply non-maximum suppression.
- Filter low-confidence detections.

Detection does not need to run on every frame. The first implementation should support a configurable interval, such as running detection every 2 or 3 frames, while reusing recent results between detections.

### FaceAligner

Normalizes detected faces before feature extraction. If landmarks are available, it should align eyes and mouth to a canonical position. If landmarks are not available in the first detector model, it should crop and resize the face region with a margin.

Responsibilities:

- Validate face box boundaries.
- Crop the face region.
- Align or resize the face to the recognizer input size.
- Reject faces that are too small or heavily clipped.

### FaceFeatureExtractor

Runs a face recognition ONNX model and returns a normalized embedding vector for each aligned face.

Responsibilities:

- Preprocess aligned faces.
- Run embedding inference.
- L2-normalize embeddings.
- Attach quality metadata used during enrollment and recognition.

### FaceDatabase

Stores enrolled identities and their feature templates locally. The first version stores only embeddings and metadata, not original face images.

Recommended storage format:

- `data/face_db.json` for human-readable metadata.
- `data/face_db.bin` or `data/face_db.npz` equivalent for compact embedding storage if JSON becomes inconvenient.

For the first version, a single JSON file is acceptable if the face count is small. Each identity should store:

- Stable ID.
- Display name.
- One or more embedding templates.
- Enrollment timestamp.
- Model name or model version.
- Embedding dimension.

### Recognizer

Compares live embeddings against enrolled templates using cosine similarity. Since embeddings are L2-normalized, cosine similarity can be computed as a dot product.

Responsibilities:

- Compare each live embedding with database templates.
- Return best match, similarity score, and unknown/person decision.
- Apply configurable thresholds.
- Support multiple templates per person by using the maximum score or averaged template score.

For small face databases, linear scan is sufficient and simple. If the database grows large, the interface should allow replacing linear scan with Faiss, HNSW, or another approximate nearest neighbor index.

### InferenceEngine

Provides a backend-neutral interface for model execution.

Initial implementation:

- `OnnxRuntimeCpuEngine`

Future implementations:

- `OnnxRuntimeCudaEngine`
- `OnnxRuntimeDirectMLEngine`
- `TensorRTEngine`

The rest of the application should not depend directly on ONNX Runtime session details.

### AppController

Coordinates lifecycle, command handling, enrollment mode, recognition mode, shutdown, and error reporting.

Responsibilities:

- Start and stop worker threads.
- Parse command-line actions.
- Manage current operating mode.
- Route frames through detection, extraction, recognition, and display.
- Persist face database changes.

## Threading Model

Use a staged pipeline with bounded queues and freshness control:

```text
CameraCapture thread
    -> latest FrameBuffer
Detection worker
    -> detected face boxes
Feature worker pool
    -> face embeddings
Recognition worker
    -> identity results
Display/main thread
    -> preview overlay and command feedback
```

The first version can use one detection worker and a small feature worker pool sized from hardware concurrency. If CPU contention hurts camera smoothness, feature workers should be capped to leave at least one core free.

Important rules:

- Camera capture must not wait for inference.
- Queues must be bounded.
- Old recognition jobs should be dropped when newer frames arrive.
- Shared data must use clear ownership: frames are copied or reference-counted `cv::Mat` objects with immutable access after publication.
- The face database should use a read-write lock or copy-on-write snapshot so recognition can continue while enrollment updates are saved.

## Enrollment Flow

Enrollment is started from the command line:

```text
enroll <name>
```

The controller enters enrollment mode:

1. Detect faces from the live camera feed.
2. Require exactly one high-quality face in frame.
3. Collect several embeddings over multiple frames, such as 10 to 20 valid samples.
4. Reject samples with low detection confidence, small face size, excessive blur, or inconsistent similarity to the current sample set.
5. Average valid embeddings and L2-normalize the result.
6. Save the template under the requested name.
7. Return to recognition mode.

If multiple faces are visible, enrollment pauses and asks the user to keep only one face in view. If no valid face is found for a timeout period, enrollment fails without changing the database. During enrollment, the preview should still draw the candidate face box and red keypoint markers so the user can see whether the algorithm is locating the face correctly.

## Recognition Flow

For each processed frame:

1. Read the freshest frame.
2. Detect face boxes on the configured interval.
3. Align or crop detected faces.
4. Extract embeddings.
5. Compare embeddings to the local database.
6. Mark faces as known if similarity is above threshold; otherwise mark as unknown.
7. Publish results to display, including face boxes and red keypoint markers for every detected face.

Initial threshold values should be configurable because the correct value depends on the selected recognition model. A reasonable first design is:

- `recognition_threshold`: minimum similarity for known identity.
- `enrollment_consistency_threshold`: minimum similarity between enrollment samples.
- `detection_threshold`: minimum detector confidence.

## Configuration

Use a simple config file, such as `config/app.yaml` or `config/app.json`.

Configuration should include:

- Camera index.
- Frame width and height.
- Detector model path.
- Recognizer model path.
- Inference backend name.
- Detection interval.
- Detection threshold.
- Recognition threshold.
- Enrollment sample count.
- Enrollment timeout.
- Worker thread counts.
- Database path.

Defaults should allow the application to run after model files are placed in the expected directory.

## Error Handling

The program should handle these failures explicitly:

- Camera cannot be opened.
- Frame capture fails repeatedly.
- Model file is missing or incompatible.
- Inference backend initialization fails.
- No face is visible during enrollment.
- More than one face is visible during enrollment.
- Database file is missing, malformed, or from a different embedding dimension.
- Database save fails.

Recoverable errors should be shown in the command-line output and preview overlay. Fatal startup errors should stop the program with a clear message.

## Performance Strategy

The first version should prioritize low latency over processing every frame.

Performance decisions:

- Drop stale frames.
- Run face detection every few frames instead of every frame.
- Extract features only from valid face crops.
- Keep model sessions initialized for the whole process.
- Avoid reallocating large buffers inside hot loops where practical.
- Use normalized embeddings so recognition is simple dot-product math.
- Keep recognition linear until database size justifies an index.

Target behavior for a normal laptop CPU:

- Smooth camera preview.
- Recognition updates several times per second.
- No visible multi-second lag when the CPU is under load.

Exact FPS depends on camera resolution, CPU, and selected models, so benchmarks should be measured after implementation.

## Data Privacy

The first version stores embeddings and metadata only. It does not store raw face images by default.

The user should be able to delete an enrolled identity. Deletion removes that identity's embedding templates from the local database. The design assumes all data stays on the local machine.

## Testing Strategy

Unit tests:

- Cosine similarity and threshold decisions.
- Face database load/save behavior.
- Enrollment sample aggregation.
- Command parsing.
- Queue freshness and bounded behavior.

Integration tests:

- Load configured models.
- Run detector preprocessing and output decoding on test images.
- Run feature extraction on test face crops.
- Verify database recognition decisions with known embeddings.

Manual tests:

- Start camera preview.
- Confirm detected faces are outlined with rectangles.
- Confirm detected face keypoints are shown as red points when landmarks are available.
- Enroll one person.
- Recognize that person.
- Verify unknown handling for a non-enrolled person.
- Delete an identity.
- Restart the app and confirm database persistence.

Performance tests:

- Measure capture FPS.
- Measure detection latency.
- Measure feature extraction latency.
- Measure end-to-end recognition latency.
- Verify behavior when the recognition pipeline is slower than camera capture.

## Initial File Layout

```text
CMakeLists.txt
README.md
config/
  app.example.json
data/
  .gitkeep
models/
  README.md
src/
  main.cpp
  app/
    AppController.hpp
    AppController.cpp
  camera/
    CameraCapture.hpp
    CameraCapture.cpp
    FrameBuffer.hpp
    FrameBuffer.cpp
  inference/
    InferenceEngine.hpp
    OnnxRuntimeCpuEngine.hpp
    OnnxRuntimeCpuEngine.cpp
  face/
    FaceDetector.hpp
    FaceDetector.cpp
    FaceAligner.hpp
    FaceAligner.cpp
    FaceFeatureExtractor.hpp
    FaceFeatureExtractor.cpp
    Recognizer.hpp
    Recognizer.cpp
    FaceDatabase.hpp
    FaceDatabase.cpp
  ui/
    PreviewWindow.hpp
    PreviewWindow.cpp
tests/
```

## Scope Boundaries

In scope for the first implementation:

- Live camera recognition.
- Live camera enrollment.
- Local face database.
- CPU ONNX Runtime inference.
- Threaded pipeline.
- Preview window overlay.
- Command-line control.

Out of scope for the first implementation:

- Qt GUI.
- Web API.
- Cloud storage.
- Remote recognition.
- Anti-spoofing or liveness detection.
- Multi-camera support.
- Automatic model download.
- Large-scale approximate nearest neighbor indexing.

These out-of-scope items can be added later without changing the core pipeline if the interfaces above are kept clean.
