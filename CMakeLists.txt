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
    src/app/AppConfig.cpp
    src/app/AppController.cpp
    src/app/AppRunCleanup.cpp
    src/app/CommandLoop.cpp
    src/app/EnrollmentSession.cpp
    src/app/RecognitionResults.cpp
    src/camera/CameraCapture.cpp
    src/camera/CameraCaptureDefault.cpp
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
    opencv_videoio
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

    add_executable(test_command_loop tests/test_command_loop.cpp)
    target_link_libraries(test_command_loop PRIVATE ccamera_core)
    add_test(NAME test_command_loop COMMAND test_command_loop)

    add_executable(test_app_config tests/test_app_config.cpp)
    target_link_libraries(test_app_config PRIVATE ccamera_core)
    add_test(NAME test_app_config COMMAND test_app_config)
    set_tests_properties(test_app_config PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(test_camera_capture_status tests/test_camera_capture_status.cpp)
    target_link_libraries(test_camera_capture_status PRIVATE ccamera_core)
    add_test(
        NAME test_camera_capture_status
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_camera_capture_status>
    )

    add_executable(test_enrollment_session tests/test_enrollment_session.cpp)
    target_link_libraries(test_enrollment_session PRIVATE ccamera_core)
    add_test(
        NAME test_enrollment_session
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_enrollment_session>
    )

    add_executable(test_recognition_results tests/test_recognition_results.cpp)
    target_link_libraries(test_recognition_results PRIVATE ccamera_core)
    add_test(NAME test_recognition_results COMMAND test_recognition_results)

    add_executable(test_model_contracts tests/test_model_contracts.cpp)
    target_link_libraries(test_model_contracts PRIVATE ccamera_core)
    add_test(NAME test_model_contracts COMMAND test_model_contracts)

    add_executable(test_app_run_cleanup tests/test_app_run_cleanup.cpp)
    target_link_libraries(test_app_run_cleanup PRIVATE ccamera_core)
    add_test(NAME test_app_run_cleanup COMMAND test_app_run_cleanup)

    add_executable(test_offline_virtual_dataset tests/test_offline_virtual_dataset.cpp)
    target_link_libraries(test_offline_virtual_dataset PRIVATE ccamera_core)
    add_test(
        NAME test_offline_virtual_dataset
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_offline_virtual_dataset>
    )
endif()
