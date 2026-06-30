if (NOT DEFINED CCAMERA_TARGET_FILE)
    message(FATAL_ERROR "CCAMERA_TARGET_FILE is required")
endif()

if (NOT DEFINED CCAMERA_CONFIG_FILE)
    message(FATAL_ERROR "CCAMERA_CONFIG_FILE is required")
endif()

set(_config_file "${CCAMERA_CONFIG_FILE}")
if (CCAMERA_STARTUP_MODE STREQUAL "missing_model")
    if (NOT DEFINED CCAMERA_STARTUP_PROBE_DIR)
        message(FATAL_ERROR "CCAMERA_STARTUP_PROBE_DIR is required for missing_model startup mode")
    endif()

    file(REMOVE_RECURSE "${CCAMERA_STARTUP_PROBE_DIR}")
    file(MAKE_DIRECTORY "${CCAMERA_STARTUP_PROBE_DIR}")
    set(_database_path "${CCAMERA_STARTUP_PROBE_DIR}/face_db.json")
    set(_config_file "${CCAMERA_STARTUP_PROBE_DIR}/app-missing-model.json")
    file(TO_CMAKE_PATH "${_database_path}" _database_path_json)
    file(WRITE "${_config_file}" [=[
{
  "camera_index": 0,
  "frame_width": 1280,
  "frame_height": 720,
  "detector_model": "models/missing-default-detector.onnx",
  "recognizer_model": "models/missing-default-recognizer.onnx",
  "database_path": "@DATABASE_PATH@",
  "database_passphrase": "startup-probe-local-database-passphrase",
  "inference_backend": "opencv_zoo",
  "detection_interval": 3,
  "detection_threshold": 0.7,
  "recognition_threshold": 0.45,
  "enrollment_consistency_threshold": 0.55,
  "enrollment_sample_count": 15,
  "enrollment_timeout_seconds": 20,
  "feature_worker_count": 2
}
]=])
    file(READ "${_config_file}" _config_json)
    string(REPLACE "@DATABASE_PATH@" "${_database_path_json}" _config_json "${_config_json}")
    file(WRITE "${_config_file}" "${_config_json}")
endif()

if (CCAMERA_STARTUP_MODE STREQUAL "runtime")
    if (NOT DEFINED CCAMERA_STARTUP_PROBE_DIR)
        message(FATAL_ERROR "CCAMERA_STARTUP_PROBE_DIR is required for runtime startup mode")
    endif()

    file(REMOVE_RECURSE "${CCAMERA_STARTUP_PROBE_DIR}")
    file(MAKE_DIRECTORY "${CCAMERA_STARTUP_PROBE_DIR}")
    set(_detector_model "${CCAMERA_STARTUP_PROBE_DIR}/dummy_detector.onnx")
    set(_recognizer_model "${CCAMERA_STARTUP_PROBE_DIR}/dummy_recognizer.onnx")
    set(_database_path "${CCAMERA_STARTUP_PROBE_DIR}/face_db.json")
    set(_config_file "${CCAMERA_STARTUP_PROBE_DIR}/app.json")
    file(WRITE "${_detector_model}" "not a real detector model\n")
    file(WRITE "${_recognizer_model}" "not a real recognizer model\n")
    file(TO_CMAKE_PATH "${_detector_model}" _detector_model_json)
    file(TO_CMAKE_PATH "${_recognizer_model}" _recognizer_model_json)
    file(TO_CMAKE_PATH "${_database_path}" _database_path_json)
    file(WRITE "${_config_file}" [=[
{
  "camera_index": 0,
  "frame_width": 1280,
  "frame_height": 720,
  "detector_model": "@DETECTOR_MODEL@",
  "recognizer_model": "@RECOGNIZER_MODEL@",
  "database_path": "@DATABASE_PATH@",
  "database_passphrase": "startup-probe-local-database-passphrase",
  "inference_backend": "onnxruntime_cpu",
  "detection_interval": 3,
  "detection_threshold": 0.7,
  "recognition_threshold": 0.45,
  "enrollment_consistency_threshold": 0.55,
  "enrollment_sample_count": 15,
  "enrollment_timeout_seconds": 20,
  "feature_worker_count": 2
}
]=])
    file(READ "${_config_file}" _config_json)
    string(REPLACE "@DETECTOR_MODEL@" "${_detector_model_json}" _config_json "${_config_json}")
    string(REPLACE "@RECOGNIZER_MODEL@" "${_recognizer_model_json}" _config_json "${_config_json}")
    string(REPLACE "@DATABASE_PATH@" "${_database_path_json}" _config_json "${_config_json}")
    file(WRITE "${_config_file}" "${_config_json}")
endif()

if (CCAMERA_STARTUP_MODE STREQUAL "default_runtime")
    if (NOT DEFINED CCAMERA_STARTUP_PROBE_DIR)
        message(FATAL_ERROR "CCAMERA_STARTUP_PROBE_DIR is required for default_runtime startup mode")
    endif()

    get_filename_component(_source_config_dir "${CCAMERA_CONFIG_FILE}" DIRECTORY)
    get_filename_component(_source_root "${_source_config_dir}" DIRECTORY)
    file(REMOVE_RECURSE "${CCAMERA_STARTUP_PROBE_DIR}")
    file(MAKE_DIRECTORY "${CCAMERA_STARTUP_PROBE_DIR}/config")
    set(_detector_model "${_source_root}/models/opencv_zoo/face_detection_yunet_2023mar.onnx")
    set(_recognizer_model "${_source_root}/models/opencv_zoo/face_recognition_sface_2021dec.onnx")
    set(_database_path "${CCAMERA_STARTUP_PROBE_DIR}/data/face_db.json")
    set(_default_config_file "${CCAMERA_STARTUP_PROBE_DIR}/config/app.example.json")
    file(TO_CMAKE_PATH "${_detector_model}" _detector_model_json)
    file(TO_CMAKE_PATH "${_recognizer_model}" _recognizer_model_json)
    file(TO_CMAKE_PATH "${_database_path}" _database_path_json)
    file(WRITE "${_default_config_file}" [=[
{
  "camera_index": 0,
  "frame_width": 1280,
  "frame_height": 720,
  "detector_model": "@DETECTOR_MODEL@",
  "recognizer_model": "@RECOGNIZER_MODEL@",
  "database_path": "@DATABASE_PATH@",
  "database_passphrase": "startup-probe-local-database-passphrase",
  "inference_backend": "opencv_zoo",
  "detection_interval": 3,
  "detection_threshold": 0.7,
  "recognition_threshold": 0.45,
  "enrollment_consistency_threshold": 0.55,
  "enrollment_sample_count": 15,
  "enrollment_timeout_seconds": 20,
  "feature_worker_count": 2
}
]=])
    file(READ "${_default_config_file}" _config_json)
    string(REPLACE "@DETECTOR_MODEL@" "${_detector_model_json}" _config_json "${_config_json}")
    string(REPLACE "@RECOGNIZER_MODEL@" "${_recognizer_model_json}" _config_json "${_config_json}")
    string(REPLACE "@DATABASE_PATH@" "${_database_path_json}" _config_json "${_config_json}")
    file(WRITE "${_default_config_file}" "${_config_json}")
endif()

set(_startup_args --config "${_config_file}")
if (CCAMERA_STARTUP_MODE STREQUAL "default_runtime")
    set(_startup_args)
endif()

execute_process(
    COMMAND "${CCAMERA_TARGET_FILE}" ${_startup_args}
    WORKING_DIRECTORY "${CCAMERA_WORKING_DIRECTORY}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
    TIMEOUT 10
)

string(STRIP "${_output}" _output)
string(STRIP "${_error}" _error)

if (_result STREQUAL "-1073741515"
        OR _result STREQUAL "0xc0000135"
        OR _result MATCHES "0xc0000135"
        OR _result MATCHES "no such file")
    message(FATAL_ERROR
        "Executable died in the Windows loader before application startup. "
        "result=${_result}; stdout='${_output}'; stderr='${_error}'"
    )
endif()

if (CCAMERA_STARTUP_MODE STREQUAL "default_runtime")
    if (_error MATCHES "detector model does not exist" OR _error MATCHES "recognizer model does not exist")
        message(FATAL_ERROR
            "Default no-argument startup did not resolve committed model paths from the executable location. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()
    if (_error MATCHES "ccamera: failed to load runtime module"
            OR _error MATCHES "ccamera: failed to find ccamera_run_app")
        message(FATAL_ERROR
            "Default no-argument startup failed before downstream app/model handling. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()

    string(TOLOWER "${_error}" _error_lower)
    set(_model_backend_error FALSE)
    if (_error_lower MATCHES "(detector|recognizer|yunet|sface).*(model|create|init|initialize|load|backend|opencv|dnn|onnx|protobuf|parse|parsing|failed)"
            OR _error_lower MATCHES "(model|create|init|initialize|load|backend|opencv|dnn|onnx|protobuf|parse|parsing|failed).*(detector|recognizer|yunet|sface)"
            OR _error_lower MATCHES "(dnn|onnx|protobuf).*(model|backend|create|init|initialize|load|read|parse|parsing|failed|error)"
            OR _error_lower MATCHES "(model|backend|create|init|initialize|load|read|parse|parsing|failed|error).*(dnn|onnx|protobuf)"
            OR _error_lower MATCHES "failed to (create|init|initialize|load).*(backend|model)"
            OR _error_lower MATCHES "(backend|model).*(failed to (create|init|initialize|load))")
        set(_model_backend_error TRUE)
    endif()
    if (_model_backend_error)
        message(FATAL_ERROR
            "Default no-argument startup failed during likely model/backend initialization. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()

    set(_camera_unavailable_error FALSE)
    if (_error_lower MATCHES "ccamera:.*(camera unavailable|camera is unavailable|no camera|failed to open camera|cannot open camera|can't open camera|unable to open camera|failed to read frame from camera)")
        set(_camera_unavailable_error TRUE)
    endif()
    if (_camera_unavailable_error)
        message(STATUS "Default no-argument startup reached downstream camera-unavailable handling: ${_error}")
        return()
    endif()
    if (_error MATCHES "ccamera:")
        message(FATAL_ERROR
            "Default no-argument startup exited with an unrecognized application error. "
            "Only timeout or known downstream camera-unavailable errors are accepted. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()
    if (_result MATCHES "timeout" OR _result MATCHES "Timeout")
        message(STATUS "Default no-argument startup stayed alive until the bounded probe timed out")
        return()
    endif()
    if (_result STREQUAL "0")
        message(FATAL_ERROR
            "Default no-argument startup exited successfully instead of staying in runtime preview. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()

    message(FATAL_ERROR
        "Default no-argument startup ended without an accepted runtime condition. "
        "Only timeout or known downstream camera-unavailable errors are accepted. "
        "result=${_result}; stdout='${_output}'; stderr='${_error}'"
    )
endif()

if (CCAMERA_STARTUP_MODE STREQUAL "runtime")
    if (_error MATCHES "detector model does not exist" OR _error MATCHES "recognizer model does not exist")
        message(FATAL_ERROR
            "Runtime startup probe did not reach runtime loading. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()
    set(_runtime_module_error FALSE)
    if (_error MATCHES "ccamera: failed to load runtime module"
            OR _error MATCHES "ccamera: failed to find ccamera_run_app")
        set(_runtime_module_error TRUE)
    endif()
    if (CCAMERA_REJECT_RUNTIME_MODULE_LOAD_FAILURE AND _runtime_module_error)
        message(FATAL_ERROR
            "Runtime startup probe failed before downstream app/model handling. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()
    if (_error MATCHES "ccamera:")
        message(STATUS "Executable reached runtime-load path or controlled application error: ${_error}")
        return()
    endif()
    if (_result EQUAL 0)
        message(FATAL_ERROR
            "Runtime startup probe exited successfully without reporting runtime-load or downstream app/model handling. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()
endif()

if (CCAMERA_STARTUP_MODE STREQUAL "missing_model")
    if (_error MATCHES "detector model does not exist: models/missing-default-detector.onnx")
        message(STATUS "Executable reported expected missing default model error: ${_error}")
        return()
    endif()
    message(FATAL_ERROR
        "Missing-model startup probe did not report the expected missing detector model. "
        "result=${_result}; stdout='${_output}'; stderr='${_error}'"
    )
endif()

if (_result EQUAL 0)
    message(STATUS "Executable started and exited successfully")
    return()
endif()

if (_error MATCHES "ccamera:")
    message(STATUS "Executable reached application error handling: ${_error}")
    return()
endif()

message(FATAL_ERROR
    "Executable did not report a recognizable application error. "
    "result=${_result}; stdout='${_output}'; stderr='${_error}'"
)
