if (NOT DEFINED CCAMERA_TARGET_FILE)
    message(FATAL_ERROR "CCAMERA_TARGET_FILE is required")
endif()

if (NOT DEFINED CCAMERA_CONFIG_FILE)
    message(FATAL_ERROR "CCAMERA_CONFIG_FILE is required")
endif()

set(_config_file "${CCAMERA_CONFIG_FILE}")
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

execute_process(
    COMMAND "${CCAMERA_TARGET_FILE}" --config "${_config_file}"
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

if (_result EQUAL 0)
    message(STATUS "Executable started and exited successfully")
    return()
endif()

if (CCAMERA_STARTUP_MODE STREQUAL "runtime")
    if (_error MATCHES "detector model does not exist" OR _error MATCHES "recognizer model does not exist")
        message(FATAL_ERROR
            "Runtime startup probe did not reach runtime loading. "
            "result=${_result}; stdout='${_output}'; stderr='${_error}'"
        )
    endif()
    if (_error MATCHES "ccamera: failed to load runtime module"
            OR _error MATCHES "ccamera:")
        message(STATUS "Executable reached runtime-load path or controlled application error: ${_error}")
        return()
    endif()
endif()

if (_error MATCHES "ccamera:")
    message(STATUS "Executable reached application error handling: ${_error}")
    return()
endif()

message(FATAL_ERROR
    "Executable did not report a recognizable application error. "
    "result=${_result}; stdout='${_output}'; stderr='${_error}'"
)
