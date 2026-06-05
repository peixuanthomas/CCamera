# Models

Place ONNX model files here:

- `face_detector.onnx`: input name `image`, batch-1 NCHW float32 tensor, OpenCV BGR channel order. Output name `detections`, shape `[N, 15]`, with rows `[x1, y1, x2, y2, score, left_eye_x, left_eye_y, right_eye_x, right_eye_y, nose_x, nose_y, mouth_left_x, mouth_left_y, mouth_right_x, mouth_right_y]` in frame pixel coordinates.
- `face_recognizer.onnx`: input name `face`, batch-1 NCHW float32 aligned face tensor, OpenCV BGR channel order. Output name `embedding`, shape `[N]` or `[1, N]`.

Model files are not committed to the repository.
