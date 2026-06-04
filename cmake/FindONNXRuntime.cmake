set(ONNXRUNTIME_ROOT "" CACHE PATH "Path to ONNX Runtime installation root")

find_path(ONNXRUNTIME_INCLUDE_DIR
    NAMES onnxruntime_cxx_api.h
    HINTS
        "${ONNXRUNTIME_ROOT}"
        "$ENV{ONNXRUNTIME_ROOT}"
    PATH_SUFFIXES include
)

find_library(ONNXRUNTIME_LIBRARY
    NAMES onnxruntime
    HINTS
        "${ONNXRUNTIME_ROOT}"
        "$ENV{ONNXRUNTIME_ROOT}"
    PATH_SUFFIXES lib lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ONNXRuntime
    REQUIRED_VARS ONNXRUNTIME_INCLUDE_DIR ONNXRUNTIME_LIBRARY
)

if (ONNXRuntime_FOUND AND NOT TARGET ONNXRuntime::ONNXRuntime)
    add_library(ONNXRuntime::ONNXRuntime UNKNOWN IMPORTED)
    set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
        IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
    )
endif()
