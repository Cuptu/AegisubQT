cmake_minimum_required(VERSION 3.20)
if(NOT APPLE)
    message(FATAL_ERROR "DeployMacMedia must run on macOS")
endif()
if(NOT BUNDLE OR NOT FFMPEG_EXECUTABLE OR NOT EXISTS "${FFMPEG_EXECUTABLE}")
    message(FATAL_ERROR "An existing BUNDLE and FFMPEG_EXECUTABLE are required")
endif()
get_filename_component(BUNDLE "${BUNDLE}" ABSOLUTE)
set(native "${BUNDLE}/Contents/Frameworks/libAstraCore.Native.dylib")
if(NOT EXISTS "${native}" OR NOT EXISTS "${BUNDLE}/Contents/MacOS/AegisubQT")
    message(FATAL_ERROR "Bundle is missing AegisubQT or the real native media library")
endif()
# The CLI is used by AudioPcmProvider. Its dependencies are part of this closure.
# Homebrew's bin/ffmpeg is a symlink into the Cellar. Copy the actual binary,
# so the bundle never retains a link back to the build machine.
file(REAL_PATH "${FFMPEG_EXECUTABLE}" ffmpeg_real)
file(COPY "${ffmpeg_real}" DESTINATION "${BUNDLE}/Contents/MacOS")
if(NOT EXISTS "${BUNDLE}/Contents/MacOS/ffmpeg")
    message(FATAL_ERROR "FFMPEG_EXECUTABLE must be named ffmpeg")
endif()

# AstraCore is loaded dynamically: explicitly seed it, rather than relying on
# the application's link graph. This copies recursive third-party dependencies,
# repairs Mach-O install names, and fails if verify_app finds external deps.
include(BundleUtilities)
# macdeployqt has already copied Qt frameworks here. BundleUtilities does not
# expand every @loader_path entry in dependent libraries' LC_RPATH commands,
# so explicitly resolve @rpath frameworks against the deployed bundle first.
set(dependency_dirs "${BUNDLE}/Contents/Frameworks" "${BUNDLE}/Contents/MacOS")
list(APPEND dependency_dirs ${MEDIA_LIBRARY_DIRS})
fixup_bundle("${BUNDLE}" "${native}" "${dependency_dirs}")
# install_name_tool invalidates existing signatures. On Apple Silicon the
# deployed FFmpeg/native-load smoke test cannot run until its dependency closure
# is signed again. Final stripping/thinning still requires a later bundle sign.
file(GLOB_RECURSE framework_files LIST_DIRECTORIES false "${BUNDLE}/Contents/Frameworks/*")
list(APPEND framework_files "${BUNDLE}/Contents/MacOS/ffmpeg")
foreach(binary IN LISTS framework_files)
    execute_process(COMMAND /usr/bin/file -b "${binary}"
        RESULT_VARIABLE file_result OUTPUT_VARIABLE file_type ERROR_VARIABLE file_error)
    if(NOT file_result EQUAL 0)
        message(FATAL_ERROR "Cannot inspect deployed file ${binary}: ${file_error}")
    endif()
    if(file_type MATCHES "Mach-O")
        execute_process(COMMAND /usr/bin/codesign --force --timestamp=none --sign - "${binary}"
            RESULT_VARIABLE sign_result OUTPUT_VARIABLE sign_output ERROR_VARIABLE sign_error)
        if(NOT sign_result EQUAL 0)
            message(FATAL_ERROR "Cannot sign deployed Mach-O ${binary}: ${sign_output}${sign_error}")
        endif()
    endif()
endforeach()
message(STATUS "Native media dependency closure deployed, verified and signed for execution; re-sign the bundle after final stripping/thinning")
