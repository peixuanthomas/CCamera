cmake_minimum_required(VERSION 3.20)
if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW)
endif()

project(CCameraFaceRecognition LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CCAMERA_BUILD_TESTS "Build local executable tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CCAMERA_LOCAL_MINGW_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third_party/msys2/mingw64")
set(CCAMERA_LOCAL_OPENCV_DIR "${CCAMERA_LOCAL_MINGW_PREFIX}/lib/cmake/opencv4")
set(CCAMERA_LOCAL_ONNXRUNTIME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/onnxruntime/onnxruntime-win-x64-1.25.1")

if (MINGW AND EXISTS "${CCAMERA_LOCAL_MINGW_PREFIX}")
    list(APPEND CMAKE_PREFIX_PATH "${CCAMERA_LOCAL_MINGW_PREFIX}")
endif()
if (EXISTS "${CCAMERA_LOCAL_ONNXRUNTIME_ROOT}")
    list(APPEND CMAKE_PREFIX_PATH "${CCAMERA_LOCAL_ONNXRUNTIME_ROOT}")
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio)
find_package(ONNXRuntime REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(ccamera_core
    src/core/Math.cpp
)

target_sources(ccamera_core PRIVATE
    src/camera/FrameBuffer.cpp
    src/face/FaceAligner.cpp
    src/face/FaceDatabase.cpp
    src/face/FaceDetector.cpp
    src/face/FaceFeatureExtractor.cpp
    src/face/Recognizer.cpp
    src/inference/OnnxRuntimeCpuEngine.cpp
    src/ui/PreviewOverlay.cpp
)

target_include_directories(ccamera_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ccamera_core PUBLIC
    opencv_core
)

target_link_libraries(ccamera_core PUBLIC
    opencv_imgproc
)

target_link_libraries(ccamera_core PUBLIC
    nlohmann_json::nlohmann_json
)

target_link_libraries(ccamera_core PUBLIC
    ONNXRuntime::ONNXRuntime
)

add_executable(ccamera
    src/main.cpp
    src/ui/PreviewWindow.cpp
)

target_link_libraries(ccamera PRIVATE
    ccamera_core
    opencv_core
    opencv_imgproc
    opencv_highgui
    opencv_videoio
    ONNXRuntime::ONNXRuntime
    nlohmann_json::nlohmann_json
)

if (CCAMERA_BUILD_TESTS)
    enable_testing()

    add_executable(test_recognizer tests/test_recognizer.cpp)
    target_link_libraries(test_recognizer PRIVATE ccamera_core)
    add_test(NAME test_recognizer COMMAND test_recognizer)

    add_executable(test_face_database tests/test_face_database.cpp)
    target_link_libraries(test_face_database PRIVATE ccamera_core)
    add_test(NAME test_face_database COMMAND test_face_database)

    add_executable(test_frame_buffer tests/test_frame_buffer.cpp)
    target_link_libraries(test_frame_buffer PRIVATE ccamera_core)
    add_test(
        NAME test_frame_buffer
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_frame_buffer>
    )

    add_executable(test_visualization_contract tests/test_visualization_contract.cpp)
    target_link_libraries(test_visualization_contract PRIVATE ccamera_core)
    add_test(
        NAME test_visualization_contract
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_visualization_contract>
    )

    add_executable(test_face_feature_extraction tests/test_face_feature_extraction.cpp)
    target_link_libraries(test_face_feature_extraction PRIVATE ccamera_core)
    add_test(
        NAME test_face_feature_extraction
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_face_feature_extraction>
    )
endif()
