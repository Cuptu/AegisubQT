cmake_minimum_required(VERSION 3.20)
foreach(required IN ITEMS LOADER NATIVE FFMPEG_EXECUTABLE WORK_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
foreach(binary IN ITEMS LOADER NATIVE FFMPEG_EXECUTABLE)
    if(NOT EXISTS "${${binary}}")
        message(FATAL_ERROR "Missing deployed ${binary}: ${${binary}}")
    endif()
endforeach()
if(UNIX AND NOT APPLE)
    # A successful dlopen on a build runner could otherwise borrow FFmpeg from
    # /usr/lib. Require the resolved FFmpeg libraries to belong to the payload.
    file(GET_RUNTIME_DEPENDENCIES LIBRARIES "${NATIVE}"
        EXECUTABLES "${FFMPEG_EXECUTABLE}"
        RESOLVED_DEPENDENCIES_VAR resolved UNRESOLVED_DEPENDENCIES_VAR missing
        CONFLICTING_DEPENDENCIES_PREFIX conflicts)
    if(missing OR conflicts_FILENAMES)
        message(FATAL_ERROR "Incomplete deployed media closure: ${missing}; conflicts: ${conflicts_FILENAMES}")
    endif()
    get_filename_component(native_dir "${NATIVE}" DIRECTORY)
    file(REAL_PATH "${native_dir}" native_dir)
    foreach(dependency IN LISTS resolved)
        get_filename_component(name "${dependency}" NAME)
        if(name MATCHES "^lib(av(codec|format|util|filter|device)|sw(scale|resample))\\.so")
            file(REAL_PATH "${dependency}" real_dependency)
            get_filename_component(dependency_dir "${real_dependency}" DIRECTORY)
            if(NOT dependency_dir STREQUAL native_dir)
                message(FATAL_ERROR "Deployed media is borrowing FFmpeg from outside its library directory: ${real_dependency}")
            endif()
        endif()
    endforeach()
endif()
get_filename_component(WORK_DIR "${WORK_DIR}" ABSOLUTE)
file(MAKE_DIRECTORY "${WORK_DIR}")
set(media "${WORK_DIR}/deployed-native-media.mkv")
# Prevent the runner's injected library paths from masking missing dependencies.
set(clean_env ${CMAKE_COMMAND} -E env --unset=LD_LIBRARY_PATH
    --unset=DYLD_LIBRARY_PATH --unset=DYLD_FALLBACK_LIBRARY_PATH)
execute_process(COMMAND ${clean_env} "${FFMPEG_EXECUTABLE}" -nostdin -v error -y
    -f lavfi -i "testsrc2=size=96x64:rate=10:duration=1"
    -f lavfi -i "sine=frequency=440:sample_rate=16000:duration=1"
    -c:v ffv1 -c:a pcm_s16le -shortest "${media}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Deployed FFmpeg failed (${result}): ${error}")
endif()
execute_process(COMMAND ${clean_env} "${LOADER}" "${NATIVE}" "${media}"
    WORKING_DIRECTORY "${WORK_DIR}" RESULT_VARIABLE result
    OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 30)
message(STATUS "${output}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Deployed native decode failed (${result}): ${error}")
endif()
