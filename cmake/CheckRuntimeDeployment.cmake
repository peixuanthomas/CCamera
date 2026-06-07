if (NOT DEFINED CCAMERA_TARGET_FILE)
    message(FATAL_ERROR "CCAMERA_TARGET_FILE is required")
endif()

if (NOT EXISTS "${CCAMERA_TARGET_FILE}")
    message(FATAL_ERROR "Target executable does not exist: ${CCAMERA_TARGET_FILE}")
endif()

if (NOT DEFINED CCAMERA_REQUIRED_DLLS)
    message(FATAL_ERROR "CCAMERA_REQUIRED_DLLS is required")
endif()

get_filename_component(_target_dir "${CCAMERA_TARGET_FILE}" DIRECTORY)
set(_missing)

if (DEFINED CCAMERA_UNRESOLVED_DLLS AND NOT CCAMERA_UNRESOLVED_DLLS STREQUAL "")
    set(_unresolved ${CCAMERA_UNRESOLVED_DLLS})
    list(REMOVE_DUPLICATES _unresolved)
    message(FATAL_ERROR
        "Runtime DLL(s) imported by deployed DLLs were not found in configured prefixes: ${_unresolved}"
    )
endif()

foreach (_dll IN LISTS CCAMERA_REQUIRED_DLLS)
    if (_dll STREQUAL "")
        continue()
    endif()

    get_filename_component(_dll_name "${_dll}" NAME)
    if (NOT EXISTS "${_target_dir}/${_dll_name}")
        list(APPEND _missing "${_dll_name}")
    endif()
endforeach()

if (_missing)
    list(REMOVE_DUPLICATES _missing)
    message(FATAL_ERROR
        "Missing runtime DLL(s) beside ${CCAMERA_TARGET_FILE}: ${_missing}"
    )
endif()

message(STATUS "Runtime DLL deployment is complete for ${CCAMERA_TARGET_FILE}")
