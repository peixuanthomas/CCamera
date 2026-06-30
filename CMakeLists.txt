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

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio objdetect)
find_package(ONNXRuntime REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

function(ccamera_append_imported_runtime_dll_name output_variable target_name)
    if (NOT TARGET "${target_name}")
        return()
    endif()

    set(_locations)
    get_target_property(_imported_configs "${target_name}" IMPORTED_CONFIGURATIONS)
    foreach (_config IN LISTS _imported_configs)
        get_target_property(_location "${target_name}" "IMPORTED_LOCATION_${_config}")
        if (_location)
            list(APPEND _locations "${_location}")
        endif()
    endforeach()

    get_target_property(_location "${target_name}" IMPORTED_LOCATION)
    if (_location)
        list(APPEND _locations "${_location}")
    endif()

    set(_names)
    foreach (_location IN LISTS _locations)
        if (_location MATCHES "\\.dll$")
            get_filename_component(_name "${_location}" NAME)
            list(APPEND _names "${_name}")
        endif()
    endforeach()

    if (_names)
        set(_current "${${output_variable}}")
        list(APPEND _current ${_names})
        list(REMOVE_DUPLICATES _current)
        set("${output_variable}" "${_current}" PARENT_SCOPE)
    endif()
endfunction()

function(ccamera_append_imported_runtime_dll output_variable target_name)
    if (NOT TARGET "${target_name}")
        return()
    endif()

    set(_locations)
    get_target_property(_imported_configs "${target_name}" IMPORTED_CONFIGURATIONS)
    foreach (_config IN LISTS _imported_configs)
        get_target_property(_location "${target_name}" "IMPORTED_LOCATION_${_config}")
        if (_location)
            list(APPEND _locations "${_location}")
        endif()
    endforeach()

    get_target_property(_location "${target_name}" IMPORTED_LOCATION)
    if (_location)
        list(APPEND _locations "${_location}")
    endif()

    set(_runtime_dlls)
    foreach (_location IN LISTS _locations)
        if (_location MATCHES "\\.dll$" AND EXISTS "${_location}")
            list(APPEND _runtime_dlls "${_location}")
        endif()
    endforeach()

    if (_runtime_dlls)
        set(_current "${${output_variable}}")
        list(APPEND _current ${_runtime_dlls})
        list(REMOVE_DUPLICATES _current)
        set("${output_variable}" "${_current}" PARENT_SCOPE)
    endif()
endfunction()

function(ccamera_append_named_runtime_dll output_variable dll_name)
    set(_runtime_dll)
    foreach (_runtime_search_dir IN LISTS CCAMERA_RUNTIME_SEARCH_DIRS)
        if (EXISTS "${_runtime_search_dir}/${dll_name}")
            set(_runtime_dll "${_runtime_search_dir}/${dll_name}")
            break()
        endif()
    endforeach()

    if (NOT _runtime_dll)
        message(FATAL_ERROR "Required runtime DLL not found: ${dll_name}")
    endif()

    set(_current "${${output_variable}}")
    list(APPEND _current "${_runtime_dll}")
    list(REMOVE_DUPLICATES _current)
    set("${output_variable}" "${_current}" PARENT_SCOPE)
endfunction()

function(ccamera_is_windows_system_dll dll_name output_variable)
    string(TOLOWER "${dll_name}" _dll_name_lower)
    set(_is_system FALSE)

    if (_dll_name_lower MATCHES "^(api-ms-win-|ext-ms-)"
            OR _dll_name_lower MATCHES "^(advapi32|authz|avicap32|avrt|bcrypt|bcryptprimitives|cfgmgr32|comctl32|comdlg32|crypt32|d3d11|d3d12|dbghelp|dnsapi|dwmapi|dwrite|dxgi|gdi32|gdiplus|imm32|iphlpapi|kernel32|mpr|msimg32|msvcp140|msvcp140_1|msvcrt|ncrypt|netapi32|ntdll|ole32|oleaut32|opengl32|powrprof|psapi|rpcrt4|secur32|setupapi|shell32|shlwapi|usp10|user32|userenv|uxtheme|vcruntime140|vcruntime140_1|version|wininet|winmm|winspool|ws2_32|wsock32|wtsapi32)\\.dll$")
        set(_is_system TRUE)
    endif()

    set("${output_variable}" "${_is_system}" PARENT_SCOPE)
endfunction()

function(ccamera_find_runtime_dll output_variable dll_name)
    set(_runtime_dll)
    foreach (_runtime_search_dir IN LISTS CCAMERA_RUNTIME_SEARCH_DIRS)
        if (EXISTS "${_runtime_search_dir}/${dll_name}")
            set(_runtime_dll "${_runtime_search_dir}/${dll_name}")
            break()
        endif()
    endforeach()

    set("${output_variable}" "${_runtime_dll}" PARENT_SCOPE)
endfunction()

function(ccamera_read_imported_dll_names output_variable dll_path)
    set(_imports)
    if (NOT CMAKE_OBJDUMP)
        set("${output_variable}" "${_imports}" PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND "${CMAKE_OBJDUMP}" -p "${dll_path}"
        RESULT_VARIABLE _objdump_result
        OUTPUT_VARIABLE _objdump_output
        ERROR_VARIABLE _objdump_error
    )

    if (NOT _objdump_result EQUAL 0)
        message(WARNING "Failed to inspect runtime DLL imports for ${dll_path}: ${_objdump_error}")
        set("${output_variable}" "${_imports}" PARENT_SCOPE)
        return()
    endif()

    string(REGEX MATCHALL "DLL Name: [^\r\n]+" _matches "${_objdump_output}")
    foreach (_match IN LISTS _matches)
        string(REGEX REPLACE "^DLL Name: +" "" _import "${_match}")
        if (NOT _import STREQUAL "")
            list(APPEND _imports "${_import}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _imports)
    set("${output_variable}" "${_imports}" PARENT_SCOPE)
endfunction()

function(ccamera_collect_transitive_runtime_dlls found_variable missing_variable)
    set(_queue ${ARGN})
    set(_visited)
    set(_found)
    set(_missing)

    while (_queue)
        list(POP_FRONT _queue _dll_path)
        if (NOT EXISTS "${_dll_path}")
            continue()
        endif()

        get_filename_component(_dll_name "${_dll_path}" NAME)
        string(TOLOWER "${_dll_name}" _dll_key)
        if (_dll_key IN_LIST _visited)
            continue()
        endif()
        list(APPEND _visited "${_dll_key}")

        ccamera_read_imported_dll_names(_imports "${_dll_path}")
        foreach (_import_name IN LISTS _imports)
            ccamera_is_windows_system_dll("${_import_name}" _is_system)
            if (_is_system)
                continue()
            endif()

            ccamera_find_runtime_dll(_import_path "${_import_name}")
            if (_import_path)
                list(APPEND _found "${_import_path}")
                list(APPEND _queue "${_import_path}")
            else()
                list(APPEND _missing "${_import_name}")
            endif()
        endforeach()
    endwhile()

    list(REMOVE_DUPLICATES _found)
    list(REMOVE_DUPLICATES _missing)
    set("${found_variable}" "${_found}" PARENT_SCOPE)
    set("${missing_variable}" "${_missing}" PARENT_SCOPE)
endfunction()

set(CCAMERA_RUNTIME_DEPLOYMENT_DLL_NAMES)
if (WIN32)
    foreach (_opencv_runtime_target IN ITEMS
            opencv_core
            opencv_highgui
            opencv_imgcodecs
            opencv_imgproc
            opencv_objdetect
            opencv_videoio)
        ccamera_append_imported_runtime_dll_name(
            CCAMERA_RUNTIME_DEPLOYMENT_DLL_NAMES
            "${_opencv_runtime_target}"
        )
    endforeach()

    list(APPEND CCAMERA_RUNTIME_DEPLOYMENT_DLL_NAMES onnxruntime.dll)
    if (MINGW)
        list(APPEND CCAMERA_RUNTIME_DEPLOYMENT_DLL_NAMES
            libgcc_s_seh-1.dll
            libwinpthread-1.dll
            libstdc++-6.dll
        )
    endif()
endif()
list(REMOVE_DUPLICATES CCAMERA_RUNTIME_DEPLOYMENT_DLL_NAMES)

get_filename_component(CCAMERA_CXX_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(CCAMERA_RUNTIME_SEARCH_DIRS)
foreach (_prefix IN LISTS CMAKE_PREFIX_PATH)
    list(APPEND CCAMERA_RUNTIME_SEARCH_DIRS
        "${_prefix}/bin"
        "${_prefix}/lib"
    )
endforeach()
if (DEFINED ENV{MINGW_PREFIX})
    list(APPEND CCAMERA_RUNTIME_SEARCH_DIRS "$ENV{MINGW_PREFIX}/bin")
endif()
if (DEFINED ENV{MSYSTEM_PREFIX})
    list(APPEND CCAMERA_RUNTIME_SEARCH_DIRS "$ENV{MSYSTEM_PREFIX}/bin")
endif()
list(APPEND CCAMERA_RUNTIME_SEARCH_DIRS
    "${CCAMERA_LOCAL_MINGW_PREFIX}/bin"
    "${CCAMERA_CXX_COMPILER_DIR}"
)
list(REMOVE_DUPLICATES CCAMERA_RUNTIME_SEARCH_DIRS)

set(CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN)
if (WIN32 AND MINGW)
    file(TO_CMAKE_PATH "${CCAMERA_LOCAL_MINGW_PREFIX}" _ccamera_local_mingw_prefix_normalized)
    get_filename_component(
        _ccamera_local_mingw_prefix_absolute
        "${_ccamera_local_mingw_prefix_normalized}"
        ABSOLUTE
    )

    set(CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN_PREFIXES)

    macro(ccamera_append_qt_platform_plugin_prefix prefix)
        if (NOT "${prefix}" STREQUAL "")
            file(TO_CMAKE_PATH "${prefix}" _qt_platform_plugin_prefix_normalized)
            get_filename_component(
                _qt_platform_plugin_prefix_absolute
                "${_qt_platform_plugin_prefix_normalized}"
                ABSOLUTE
            )
            if (NOT _qt_platform_plugin_prefix_absolute IN_LIST CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN_PREFIXES)
                list(APPEND CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN_PREFIXES "${_qt_platform_plugin_prefix_absolute}")
            endif()
        endif()
    endmacro()

    macro(ccamera_append_nonlocal_qt_platform_plugin_prefix prefix)
        if (NOT "${prefix}" STREQUAL "")
            file(TO_CMAKE_PATH "${prefix}" _qt_platform_plugin_prefix_normalized)
            get_filename_component(
                _qt_platform_plugin_prefix_absolute
                "${_qt_platform_plugin_prefix_normalized}"
                ABSOLUTE
            )
            if (NOT _qt_platform_plugin_prefix_absolute STREQUAL _ccamera_local_mingw_prefix_absolute)
                ccamera_append_qt_platform_plugin_prefix("${_qt_platform_plugin_prefix_absolute}")
            endif()
        endif()
    endmacro()

    if (OpenCV_DIR AND NOT OpenCV_DIR MATCHES "-NOTFOUND$")
        file(TO_CMAKE_PATH "${OpenCV_DIR}" _opencv_package_dir)
        if (_opencv_package_dir MATCHES "/(lib|share)/cmake/[^/]+$")
            string(REGEX REPLACE "/(lib|share)/cmake/[^/]+$" "" _opencv_prefix "${_opencv_package_dir}")
        elseif (_opencv_package_dir MATCHES "/(lib|share)/[^/]+$")
            string(REGEX REPLACE "/(lib|share)/[^/]+$" "" _opencv_prefix "${_opencv_package_dir}")
        elseif (_opencv_package_dir MATCHES "/lib$")
            string(REGEX REPLACE "/lib$" "" _opencv_prefix "${_opencv_package_dir}")
        else()
            set(_opencv_prefix "${_opencv_package_dir}")
        endif()
        ccamera_append_nonlocal_qt_platform_plugin_prefix("${_opencv_prefix}")
    endif()

    foreach (_prefix IN LISTS CMAKE_PREFIX_PATH)
        ccamera_append_nonlocal_qt_platform_plugin_prefix("${_prefix}")
    endforeach()

    if (DEFINED ENV{MINGW_PREFIX})
        ccamera_append_nonlocal_qt_platform_plugin_prefix("$ENV{MINGW_PREFIX}")
    endif()
    if (DEFINED ENV{MSYSTEM_PREFIX})
        ccamera_append_nonlocal_qt_platform_plugin_prefix("$ENV{MSYSTEM_PREFIX}")
    endif()

    ccamera_append_qt_platform_plugin_prefix("${CCAMERA_LOCAL_MINGW_PREFIX}")

    set(_qt_platform_plugin_relative_paths
        share/qt6/plugins/platforms/qwindows.dll
        lib/qt6/plugins/platforms/qwindows.dll
        plugins/platforms/qwindows.dll
        qt6/plugins/platforms/qwindows.dll
    )
    foreach (_prefix IN LISTS CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN_PREFIXES)
        foreach (_relative_path IN LISTS _qt_platform_plugin_relative_paths)
            if (EXISTS "${_prefix}/${_relative_path}")
                set(CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN "${_prefix}/${_relative_path}")
                break()
            endif()
        endforeach()
        if (CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN)
            break()
        endif()
    endforeach()

    if (NOT EXISTS "${CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN}")
        string(REPLACE ";" "\n  " _qt_platform_plugin_prefixes "${CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN_PREFIXES}")
        string(REPLACE ";" "\n  " _qt_platform_plugin_layouts "${_qt_platform_plugin_relative_paths}")
        message(FATAL_ERROR
            "Required Qt Windows platform plugin qwindows.dll was not found. "
            "Searched dependency prefixes in order:\n  ${_qt_platform_plugin_prefixes}\n"
            "With relative layouts:\n  ${_qt_platform_plugin_layouts}"
        )
    endif()
endif()

add_library(ccamera_config
    src/app/AppConfig.cpp
    src/app/StartupMode.cpp
)

target_include_directories(ccamera_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ccamera_config PUBLIC
    nlohmann_json::nlohmann_json
)

add_library(ccamera_core
    src/core/Math.cpp
)

target_sources(ccamera_core PRIVATE
    src/app/AppController.cpp
    src/app/AppRunCleanup.cpp
    src/app/CommandLoop.cpp
    src/app/EnrollmentSession.cpp
    src/app/FrameRecognition.cpp
    src/app/RecognitionResults.cpp
    src/camera/CameraCapture.cpp
    src/camera/CameraCaptureDefault.cpp
    src/camera/FrameBuffer.cpp
    src/face/DatabaseCrypto.cpp
    src/face/FaceAligner.cpp
    src/face/FaceDatabase.cpp
    src/face/FaceDetector.cpp
    src/face/FaceFeatureExtractor.cpp
    src/face/FaceServices.cpp
    src/face/InsightFaceArcFaceFeatureExtractor.cpp
    src/face/OpenCvZooFaceDetector.cpp
    src/face/OpenCvZooFaceFeatureExtractor.cpp
    src/face/Recognizer.cpp
    src/inference/OnnxRuntimeCpuEngine.cpp
    src/ui/RuntimeControls.cpp
    src/ui/PreviewOverlay.cpp
    src/web/WebCommandQueue.cpp
    src/web/WebStatus.cpp
    src/web/Http.cpp
    src/web/RuntimeWebRouter.cpp
    src/web/MjpegStream.cpp
    src/web/RuntimeWebServer.cpp
)

target_include_directories(ccamera_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ccamera_core PUBLIC
    ccamera_config
)

target_link_libraries(ccamera_core PUBLIC
    opencv_core
)

target_link_libraries(ccamera_core PUBLIC
    opencv_imgproc
)

target_link_libraries(ccamera_core PUBLIC
    opencv_imgcodecs
)

target_link_libraries(ccamera_core PUBLIC
    opencv_objdetect
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

if (WIN32)
    target_link_libraries(ccamera_core PUBLIC ws2_32 bcrypt)
endif()

add_executable(ccamera
    src/main.cpp
)

target_link_libraries(ccamera PRIVATE
    ccamera_config
)

target_compile_definitions(ccamera PRIVATE
    "CCAMERA_RUNTIME_MODULE_NAME=\"$<TARGET_FILE_NAME:ccamera_runtime>\""
)

add_library(ccamera_runtime SHARED
    src/runtime/RunApp.cpp
    src/ui/PreviewWindow.cpp
)

if (WIN32)
    set_target_properties(ccamera_runtime PROPERTIES PREFIX "")
endif()

set(CCAMERA_RUNTIME_DEPLOYMENT_CHECK_NAMES "${CCAMERA_RUNTIME_DEPLOYMENT_DLL_NAMES}")
set(CCAMERA_RUNTIME_DEPLOYMENT_CHECK_RELATIVE_PATHS)
if (WIN32)
    list(APPEND CCAMERA_RUNTIME_DEPLOYMENT_CHECK_NAMES "$<TARGET_FILE_NAME:ccamera_runtime>")
endif()
if (WIN32 AND MINGW)
    list(APPEND CCAMERA_RUNTIME_DEPLOYMENT_CHECK_RELATIVE_PATHS "platforms/qwindows.dll")
endif()

target_link_libraries(ccamera_runtime PRIVATE
    ccamera_core
    opencv_core
    opencv_imgproc
    opencv_highgui
    opencv_videoio
    ONNXRuntime::ONNXRuntime
    nlohmann_json::nlohmann_json
)

add_dependencies(ccamera ccamera_runtime)

set(CCAMERA_REQUIRED_RUNTIME_DLLS)
if (WIN32)
    foreach (_opencv_runtime_target IN ITEMS
            opencv_core
            opencv_highgui
            opencv_imgcodecs
            opencv_imgproc
            opencv_objdetect
            opencv_videoio)
        ccamera_append_imported_runtime_dll(
            CCAMERA_REQUIRED_RUNTIME_DLLS
            "${_opencv_runtime_target}"
        )
    endforeach()
    ccamera_append_imported_runtime_dll(
        CCAMERA_REQUIRED_RUNTIME_DLLS
        ONNXRuntime::ONNXRuntime
    )

    if (MINGW)
        foreach (_mingw_runtime_dll IN ITEMS
                libgcc_s_seh-1.dll
                libwinpthread-1.dll
                libstdc++-6.dll)
            ccamera_append_named_runtime_dll(
                CCAMERA_REQUIRED_RUNTIME_DLLS
                "${_mingw_runtime_dll}"
            )
        endforeach()
    endif()
endif()
list(REMOVE_DUPLICATES CCAMERA_REQUIRED_RUNTIME_DLLS)

set(CCAMERA_UNRESOLVED_RUNTIME_DLLS)
if (WIN32 AND CMAKE_OBJDUMP AND CCAMERA_REQUIRED_RUNTIME_DLLS)
    ccamera_collect_transitive_runtime_dlls(
        CCAMERA_TRANSITIVE_RUNTIME_DLLS
        CCAMERA_UNRESOLVED_RUNTIME_DLLS
        ${CCAMERA_REQUIRED_RUNTIME_DLLS}
    )
    list(APPEND CCAMERA_REQUIRED_RUNTIME_DLLS ${CCAMERA_TRANSITIVE_RUNTIME_DLLS})
    list(REMOVE_DUPLICATES CCAMERA_REQUIRED_RUNTIME_DLLS)

    foreach (_runtime_dll IN LISTS CCAMERA_TRANSITIVE_RUNTIME_DLLS)
        get_filename_component(_runtime_dll_name "${_runtime_dll}" NAME)
        list(APPEND CCAMERA_RUNTIME_DEPLOYMENT_CHECK_NAMES "${_runtime_dll_name}")
    endforeach()
    list(REMOVE_DUPLICATES CCAMERA_RUNTIME_DEPLOYMENT_CHECK_NAMES)
elseif (WIN32 AND NOT CMAKE_OBJDUMP)
    message(WARNING "CMAKE_OBJDUMP is unavailable; transitive Windows runtime DLL deployment cannot be inspected")
endif()

if (WIN32 AND CCAMERA_REQUIRED_RUNTIME_DLLS)
    add_custom_command(TARGET ccamera_runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CCAMERA_REQUIRED_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:ccamera_runtime>
        COMMAND_EXPAND_LISTS
        COMMENT "Deploying runtime DLLs beside ccamera"
    )
endif()

if (WIN32 AND MINGW)
    add_custom_command(TARGET ccamera_runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:ccamera_runtime>/platforms
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CCAMERA_QT_WINDOWS_PLATFORM_PLUGIN}"
            $<TARGET_FILE_DIR:ccamera_runtime>/platforms
        COMMENT "Deploying Qt Windows platform plugin beside ccamera"
    )
endif()

if (CCAMERA_BUILD_TESTS)
    enable_testing()

    add_executable(test_startup_mode tests/test_startup_mode.cpp)
    target_link_libraries(test_startup_mode PRIVATE ccamera_config)
    add_test(NAME test_startup_mode COMMAND test_startup_mode)

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

    add_executable(
        test_insightface_arcface_feature_extraction
        tests/test_insightface_arcface_feature_extraction.cpp
    )
    target_link_libraries(test_insightface_arcface_feature_extraction PRIVATE ccamera_core)
    add_test(
        NAME test_insightface_arcface_feature_extraction
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_insightface_arcface_feature_extraction>
    )

    add_executable(test_opencv_zoo_adapters tests/test_opencv_zoo_adapters.cpp)
    target_link_libraries(test_opencv_zoo_adapters PRIVATE ccamera_core)
    add_test(
        NAME test_opencv_zoo_adapters
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_opencv_zoo_adapters>
    )

    add_executable(test_command_loop tests/test_command_loop.cpp)
    target_link_libraries(test_command_loop PRIVATE ccamera_core)
    add_test(NAME test_command_loop COMMAND test_command_loop)

    add_executable(test_web_command_queue tests/test_web_command_queue.cpp)
    target_link_libraries(test_web_command_queue PRIVATE ccamera_core)
    add_test(NAME test_web_command_queue COMMAND test_web_command_queue)

    add_executable(test_web_status tests/test_web_status.cpp)
    target_link_libraries(test_web_status PRIVATE ccamera_core)
    add_test(NAME test_web_status COMMAND test_web_status)

    add_executable(test_http_router tests/test_http_router.cpp)
    target_link_libraries(test_http_router PRIVATE ccamera_core)
    add_test(NAME test_http_router COMMAND test_http_router)

    add_executable(test_mjpeg_stream tests/test_mjpeg_stream.cpp)
    target_link_libraries(test_mjpeg_stream PRIVATE ccamera_core)
    add_test(
        NAME test_mjpeg_stream
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_mjpeg_stream>
    )

    add_executable(test_runtime_web_server tests/test_runtime_web_server.cpp)
    target_link_libraries(test_runtime_web_server PRIVATE ccamera_core)
    add_test(NAME test_runtime_web_server COMMAND test_runtime_web_server)

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

    add_executable(test_runtime_controls tests/test_runtime_controls.cpp)
    target_link_libraries(test_runtime_controls PRIVATE ccamera_core)
    add_test(NAME test_runtime_controls COMMAND test_runtime_controls)

    add_executable(test_model_contracts tests/test_model_contracts.cpp)
    target_link_libraries(test_model_contracts PRIVATE ccamera_core)
    add_test(NAME test_model_contracts COMMAND test_model_contracts)

    add_executable(test_app_run_cleanup tests/test_app_run_cleanup.cpp)
    target_link_libraries(test_app_run_cleanup PRIVATE ccamera_core)
    add_test(NAME test_app_run_cleanup COMMAND test_app_run_cleanup)

    add_executable(test_app_web_commands tests/test_app_web_commands.cpp)
    target_compile_definitions(test_app_web_commands PRIVATE CCAMERA_TEST_HOOKS)
    target_link_libraries(test_app_web_commands PRIVATE ccamera_core)
    add_test(NAME test_app_web_commands COMMAND test_app_web_commands)
    set_tests_properties(test_app_web_commands PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(test_offline_virtual_dataset tests/test_offline_virtual_dataset.cpp)
    target_link_libraries(test_offline_virtual_dataset PRIVATE ccamera_core)
    add_test(
        NAME test_offline_virtual_dataset
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_offline_virtual_dataset>
    )

    if (WIN32)
        add_test(
            NAME test_runtime_deployment
            COMMAND ${CMAKE_COMMAND}
                -DCCAMERA_TARGET_FILE=$<TARGET_FILE:ccamera>
                "-DCCAMERA_REQUIRED_DLLS=${CCAMERA_RUNTIME_DEPLOYMENT_CHECK_NAMES}"
                "-DCCAMERA_REQUIRED_RELATIVE_PATHS=${CCAMERA_RUNTIME_DEPLOYMENT_CHECK_RELATIVE_PATHS}"
                "-DCCAMERA_UNRESOLVED_DLLS=${CCAMERA_UNRESOLVED_RUNTIME_DLLS}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckRuntimeDeployment.cmake"
        )
        add_test(
            NAME test_executable_startup
            COMMAND ${CMAKE_COMMAND}
                -DCCAMERA_TARGET_FILE=$<TARGET_FILE:ccamera>
                -DCCAMERA_CONFIG_FILE=${CMAKE_CURRENT_SOURCE_DIR}/config/app.example.json
                -DCCAMERA_WORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}
                -DCCAMERA_STARTUP_MODE=missing_model
                -DCCAMERA_STARTUP_PROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}/manual-startup-probe
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckExecutableStartup.cmake"
        )
        add_test(
            NAME test_executable_startup_default_runtime
            COMMAND ${CMAKE_COMMAND}
                -DCCAMERA_TARGET_FILE=$<TARGET_FILE:ccamera>
                -DCCAMERA_CONFIG_FILE=${CMAKE_CURRENT_SOURCE_DIR}/config/app.example.json
                -DCCAMERA_WORKING_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/default-runtime-probe
                -DCCAMERA_STARTUP_MODE=default_runtime
                -DCCAMERA_STARTUP_PROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}/default-runtime-probe
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckExecutableStartup.cmake"
        )
        add_test(
            NAME test_executable_startup_runtime
            COMMAND ${CMAKE_COMMAND}
                -DCCAMERA_TARGET_FILE=$<TARGET_FILE:ccamera>
                -DCCAMERA_CONFIG_FILE=${CMAKE_CURRENT_SOURCE_DIR}/config/app.example.json
                -DCCAMERA_WORKING_DIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}
                -DCCAMERA_STARTUP_MODE=runtime
                -DCCAMERA_STARTUP_PROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}/startup-runtime-probe
                -DCCAMERA_REJECT_RUNTIME_MODULE_LOAD_FAILURE=ON
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckExecutableStartup.cmake"
        )
        add_test(
            NAME test_find_onnxruntime_bin_layout
            COMMAND ${CMAKE_COMMAND}
                -DCCAMERA_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
                -DCCAMERA_PROBE_DIR=${CMAKE_CURRENT_BINARY_DIR}/onnxruntime-bin-layout-probe
                "-DCCAMERA_GENERATOR=${CMAKE_GENERATOR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestFindONNXRuntimeBinLayout.cmake"
        )
    endif()
endif()
