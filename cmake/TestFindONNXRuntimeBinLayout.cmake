if (NOT DEFINED CCAMERA_SOURCE_DIR)
    message(FATAL_ERROR "CCAMERA_SOURCE_DIR is required")
endif()

if (NOT DEFINED CCAMERA_PROBE_DIR)
    message(FATAL_ERROR "CCAMERA_PROBE_DIR is required")
endif()

file(REMOVE_RECURSE "${CCAMERA_PROBE_DIR}")
file(MAKE_DIRECTORY
    "${CCAMERA_PROBE_DIR}/source"
    "${CCAMERA_PROBE_DIR}/onnxruntime/include"
    "${CCAMERA_PROBE_DIR}/onnxruntime/lib"
    "${CCAMERA_PROBE_DIR}/onnxruntime/bin"
)

file(WRITE "${CCAMERA_PROBE_DIR}/onnxruntime/include/onnxruntime_cxx_api.h" "\n")
file(WRITE "${CCAMERA_PROBE_DIR}/onnxruntime/lib/onnxruntime.lib" "\n")
file(WRITE "${CCAMERA_PROBE_DIR}/onnxruntime/bin/onnxruntime.dll" "\n")

file(WRITE "${CCAMERA_PROBE_DIR}/source/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.20)
if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()
project(ONNXRuntimeBinLayoutProbe LANGUAGES NONE)

list(APPEND CMAKE_MODULE_PATH "${CCAMERA_SOURCE_DIR}/cmake")
set(ONNXRUNTIME_ROOT "${CCAMERA_PROBE_DIR}/onnxruntime")

find_package(ONNXRuntime REQUIRED)
get_target_property(_runtime ONNXRuntime::ONNXRuntime IMPORTED_LOCATION)

if (NOT _runtime MATCHES "bin[/\\]onnxruntime\\.dll$")
    message(FATAL_ERROR "ONNXRuntime runtime DLL was not resolved from bin layout: ${_runtime}")
endif()
]=])

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -G "${CCAMERA_GENERATOR}"
        -S "${CCAMERA_PROBE_DIR}/source"
        -B "${CCAMERA_PROBE_DIR}/build"
        -DCCAMERA_SOURCE_DIR=${CCAMERA_SOURCE_DIR}
        -DCCAMERA_PROBE_DIR=${CCAMERA_PROBE_DIR}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error
)

if (NOT _result EQUAL 0)
    message(FATAL_ERROR
        "ONNX Runtime bin-layout probe failed. stdout='${_output}' stderr='${_error}'"
    )
endif()
