# InsightFace Model Sources

This directory is reserved for optional InsightFace recognizer models. The
default application configuration still uses the committed OpenCV Zoo models in
`models/opencv_zoo/`.

## buffalo_l ArcFace

- Project model zoo: https://github.com/deepinsight/insightface/tree/master/model_zoo
- Release package: https://github.com/deepinsight/insightface/releases/tag/v0.7
- Archive: `buffalo_l.zip`
- File inside archive: `w600k_r50.onnx`
- Expected local path: `models/insightface/buffalo_l/w600k_r50.onnx`
- Intended use: non-commercial research use only, following the upstream
  InsightFace model package terms.
- SHA256: TODO after the exact archive/model file is downloaded and approved.

Do not mix embeddings produced by this model with embeddings produced by OpenCV
Zoo SFace or custom recognizer contracts. Re-enroll identities into a separate
database such as `data/face_db_insightface.json`.
