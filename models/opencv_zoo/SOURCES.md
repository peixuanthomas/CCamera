# OpenCV Zoo Model Sources

This directory contains default models used by CCamera's `opencv_zoo` backend.

## Files

- `face_detection_yunet_2023mar.onnx`
  - Source: https://github.com/opencv/opencv_zoo/tree/main/models/face_detection_yunet
  - Direct download used by this project: https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/face_detection_yunet_2023mar.onnx
  - License: MIT, copied to `LICENSE.yunet.txt`.

- `face_recognition_sface_2021dec.onnx`
  - Source: https://github.com/opencv/opencv_zoo/tree/main/models/face_recognition_sface
  - Direct download used by this project: https://github.com/opencv/opencv_zoo/raw/main/models/face_recognition_sface/face_recognition_sface_2021dec.onnx
  - License: Apache 2.0, copied to `LICENSE.sface.txt`.

These files are committed so the default Windows build can run without asking the user to provide model files.
