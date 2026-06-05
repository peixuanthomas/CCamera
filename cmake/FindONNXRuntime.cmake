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

if (ONNXRUNTIME_LIBRARY)
    get_filename_component(_onnxruntime_library_dir "${ONNXRUNTIME_LIBRARY}" DIRECTORY)
endif()

find_file(ONNXRUNTIME_RUNTIME_LIBRARY
    NAMES onnxruntime.dll
    HINTS
        "${_onnxruntime_library_dir}"
        "${ONNXRUNTIME_ROOT}"
        "$ENV{ONNXRUNTIME_ROOT}"
    PATH_SUFFIXES bin lib lib64
    NO_DEFAULT_PATH
)

if (NOT ONNXRUNTIME_RUNTIME_LIBRARY)
    find_file(ONNXRUNTIME_RUNTIME_LIBRARY
        NAMES onnxruntime.dll
        HINTS
            "${_onnxruntime_library_dir}"
            "${ONNXRUNTIME_ROOT}"
            "$ENV{ONNXRUNTIME_ROOT}"
        PATH_SUFFIXES bin lib lib64
    )
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ONNXRuntime
    REQUIRED_VARS ONNXRUNTIME_INCLUDE_DIR ONNXRUNTIME_LIBRARY
)

if (ONNXRuntime_FOUND AND NOT TARGET ONNXRuntime::ONNXRuntime)
    add_library(ONNXRuntime::ONNXRuntime UNKNOWN IMPORTED)
    if (ONNXRUNTIME_RUNTIME_LIBRARY)
        set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
            IMPORTED_IMPLIB "${ONNXRUNTIME_LIBRARY}"
            IMPORTED_LOCATION "${ONNXRUNTIME_RUNTIME_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
        )
    else()
        set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
            IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
        )
    endif()
endif()
