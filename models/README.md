# Models

Place ONNX model files here:

- `face_detector.onnx`: detector output rows must be `[x1, y1, x2, y2, score, left_eye_x, left_eye_y, right_eye_x, right_eye_y, nose_x, nose_y, mouth_left_x, mouth_left_y, mouth_right_x, mouth_right_y]`.
- `face_recognizer.onnx`: recognizer output must be one embedding vector per aligned face.

Model files are not committed to the repository.
