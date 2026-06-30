# Models

The default models are still the committed OpenCV Zoo models under
`models/opencv_zoo/`:

- `opencv_zoo/face_detection_yunet_2023mar.onnx`
- `opencv_zoo/face_recognition_sface_2021dec.onnx`

See `models/opencv_zoo/SOURCES.md` for original source URLs and license notes.

The optional InsightFace example config uses the same YuNet detector and a
separate ArcFace recognizer path:

- `insightface/buffalo_l/w600k_r50.onnx`

The InsightFace model is not interchangeable with OpenCV Zoo SFace. Keep its
embeddings in a separate database such as `data/face_db_insightface.json`,
re-enroll identities, and recalibrate recognition thresholds for that backend.
See `models/insightface/SOURCES.md` before downloading or using the model.

Custom ONNX Runtime models are still supported through `--config`, but they must
match the custom `onnxruntime_cpu` contract documented in the project README.
That custom contract is separate from the InsightFace ArcFace contract.
