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

target_include_directories(ccamera_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ccamera_core PUBLIC
    opencv_core
)

add_executable(ccamera
    src/main.cpp
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
endif()
