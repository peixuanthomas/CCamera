# Models

The default models are committed under `models/opencv_zoo/`:

- `opencv_zoo/face_detection_yunet_2023mar.onnx`
- `opencv_zoo/face_recognition_sface_2021dec.onnx`

See `models/opencv_zoo/SOURCES.md` for original source URLs and license notes.

Custom ONNX Runtime models are still supported through `--config`, but they must match the custom `onnxruntime_cpu` contract documented in the project README.
